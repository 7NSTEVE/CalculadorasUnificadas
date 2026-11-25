import XCTest
import Foundation
import CryptoKit
@testable import CuadernUEstudio

final class ImportacionOneNoteIPadTests: XCTestCase {
    func testDetectorExtensionesNoDependeDeMayusculas() {
        XCTAssertEqual(FormatoImportacionOneNote(url: URL(fileURLWithPath: "/tmp/A.ONE")), .one)
        XCTAssertEqual(FormatoImportacionOneNote(url: URL(fileURLWithPath: "/tmp/A.OnePkg")), .onepkg)
        XCTAssertEqual(FormatoImportacionOneNote(url: URL(fileURLWithPath: "/tmp/A.ONETOC2")), .onetoc2)
        XCTAssertEqual(FormatoImportacionOneNote(url: URL(fileURLWithPath: "/tmp/A.zip")), .desconocido)
    }

    func testOnePkgReconocidoPeroNoPrometeParserOneStore() {
        XCTAssertEqual(FormatoImportacionOneNote.one.nivelSoporte, .revisionStoreDirecto)
        XCTAssertEqual(FormatoImportacionOneNote.onepkg.nivelSoporte, .paqueteReconocido)
        XCTAssertEqual(FormatoImportacionOneNote.onetoc2.nivelSoporte, .tablaContenidosReconocida)
    }

    func testSanitizadorImpideTraversalYSeparadores() {
        let limpio = SanitizadorNombresImportacionOneNote.nombreSeguro("../../Tema\\final:\u{0000}.one")
        XCTAssertFalse(limpio.contains(".."))
        XCTAssertFalse(limpio.contains("/"))
        XCTAssertFalse(limpio.contains("\\"))
        XCTAssertFalse(limpio.contains(":"))
        XCTAssertFalse(limpio.isEmpty)
    }

    func testUUIDDeterministaEsEstableYSensibleARuta() {
        let sha = String(repeating: "ab", count: 32)
        let a1 = IdentidadImportacionOneNote.uuidDeterminista(sha256Fuente: sha, rutaLogica: "section/1/page/2")
        let a2 = IdentidadImportacionOneNote.uuidDeterminista(sha256Fuente: sha, rutaLogica: "section/1/page/2")
        let b = IdentidadImportacionOneNote.uuidDeterminista(sha256Fuente: sha, rutaLogica: "section/1/page/3")
        XCTAssertEqual(a1, a2)
        XCTAssertNotEqual(a1, b)
    }

    func testInspectorAceptaHeaderOneOficial() throws {
        let guidArchivo = UUID(uuidString: "8DFAB807-A83A-4AC0-9393-E1DD0691CFB8")!
        let datos = fabricarHeader(
            tipo: InspectorOneStoreIPad.guidTipoOne,
            archivo: guidArchivo,
            formato: InspectorOneStoreIPad.guidRevisionStore,
            version: 0x2A
        )
        let resultado = try InspectorOneStoreIPad.inspeccionar(
            prefijo: datos,
            tamanoTotal: 4096,
            extensionArchivo: "one"
        )
        XCTAssertEqual(resultado.tipo, .seccionOne)
        XCTAssertEqual(resultado.guidArchivo, guidArchivo)
        XCTAssertEqual(resultado.versionUltimoEscritor, 0x2A)
    }

    func testInspectorAceptaOneToc2YRechazaExtensionMentira() throws {
        let datos = fabricarHeader(
            tipo: InspectorOneStoreIPad.guidTipoOneToc2,
            archivo: UUID(),
            formato: InspectorOneStoreIPad.guidRevisionStore,
            version: 0x1B
        )
        XCTAssertNoThrow(try InspectorOneStoreIPad.inspeccionar(
            prefijo: datos,
            tamanoTotal: 4096,
            extensionArchivo: "onetoc2"
        ))
        XCTAssertThrowsError(try InspectorOneStoreIPad.inspeccionar(
            prefijo: datos,
            tamanoTotal: 4096,
            extensionArchivo: "one"
        )) { error in
            guard case ErrorInspeccionOneStore.extensionNoCoincide = error else {
                return XCTFail("Error inesperado: \(error)")
            }
        }
    }

    func testInspectorRechazaRevisionStoreFalso() {
        let datos = fabricarHeader(
            tipo: InspectorOneStoreIPad.guidTipoOne,
            archivo: UUID(),
            formato: UUID(),
            version: 0x2A
        )
        XCTAssertThrowsError(try InspectorOneStoreIPad.inspeccionar(
            prefijo: datos,
            tamanoTotal: 4096,
            extensionArchivo: "one"
        ))
    }

    func testHashStreamingCoincideConCryptoKit() throws {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("CuadernU-hash-\(UUID().uuidString).one")
        defer { try? FileManager.default.removeItem(at: url) }
        let datos = Data((0..<65_537).map { UInt8($0 % 251) })
        try datos.write(to: url)

        let obtenido = try AlmacenTemporalImportacionOneNote.sha256Streaming(
            url: url,
            tamanoBloque: 1024
        )
        let esperado = SHA256.hash(data: datos).map { String(format: "%02x", $0) }.joined()
        XCTAssertEqual(obtenido, esperado)
    }

    func testStagingCopiaSinAlterarFuenteYSePuedeLimpiar() async throws {
        let original = FileManager.default.temporaryDirectory
            .appendingPathComponent("CuadernU-stage-\(UUID().uuidString).one")
        let contenido = Data(repeating: 0x5A, count: 8192)
        try contenido.write(to: original)
        defer { try? FileManager.default.removeItem(at: original) }

        let preparado = try await AlmacenTemporalImportacionOneNote.compartido.preparar(urlOriginal: original)
        XCTAssertTrue(FileManager.default.fileExists(atPath: preparado.archivoPreparado.path))
        XCTAssertEqual(try Data(contentsOf: preparado.archivoPreparado), contenido)
        XCTAssertEqual(try Data(contentsOf: original), contenido)

        let carpeta = preparado.carpetaTrabajo
        await AlmacenTemporalImportacionOneNote.compartido.limpiar(preparado)
        XCTAssertFalse(FileManager.default.fileExists(atPath: carpeta.path))
    }

    func testRegistroReabreISO8601YDeduplicaPorSHA() async throws {
        let carpeta = FileManager.default.temporaryDirectory
            .appendingPathComponent("CuadernU-registro-\(UUID().uuidString)", isDirectory: true)
        let url = carpeta.appendingPathComponent("registro.json")
        defer { try? FileManager.default.removeItem(at: carpeta) }

        let huella = HuellaFuenteOneNote(
            sha256: String(repeating: "cd", count: 32),
            tamanoBytes: 1234,
            fechaModificacion: Date(timeIntervalSince1970: 1_700_000_000),
            nombreOriginal: "Circuitos.one",
            extensionOriginal: "one"
        )
        let repo1 = RepositorioRegistroImportacionesOneNote(url: url)
        let inicial = RegistroImportacionOneNote(
            huella: huella,
            formato: .one,
            estado: .inspeccionada
        )
        let guardado = try await repo1.guardar(inicial)

        let repo2 = RepositorioRegistroImportacionesOneNote(url: url)
        let reabierto = await repo2.registro(sha256: huella.sha256)
        XCTAssertEqual(reabierto?.id, guardado.id)
        XCTAssertEqual(reabierto?.huella.fechaModificacion, huella.fechaModificacion)

        var reintento = inicial
        reintento.id = UUID()
        reintento.estado = .requiereCompatibilidad
        let actualizado = try await repo2.guardar(reintento)
        XCTAssertEqual(actualizado.id, guardado.id)
        XCTAssertEqual((await repo2.todos()).count, 1)
    }

    func testInformeFidelidadNoDeclaraCompletoSiPreservaDesconocidos() {
        let informe = InformeFidelidadImportacionOneNote(
            elementosReconocidos: 10,
            elementosConvertidos: 9,
            elementosPreservadosSinConvertir: 1,
            elementosNoSoportados: 0,
            advertencias: []
        )
        XCTAssertTrue(informe.requiereRevision)
        XCTAssertFalse(informe.conversionCompletaConocida)
    }

    // MARK: Helpers

    private func fabricarHeader(
        tipo: UUID,
        archivo: UUID,
        formato: UUID,
        version: UInt32
    ) -> Data {
        var datos = Data(repeating: 0, count: 80)
        escribirGUIDWindows(tipo, en: &datos, offset: 0x00)
        escribirGUIDWindows(archivo, en: &datos, offset: 0x10)
        escribirGUIDWindows(formato, en: &datos, offset: 0x30)
        for offset in [0x40, 0x44, 0x48, 0x4C] {
            escribirUInt32LE(version, en: &datos, offset: offset)
        }
        return datos
    }

    private func escribirGUIDWindows(_ uuid: UUID, en datos: inout Data, offset: Int) {
        let u = uuid.uuid
        let b: [UInt8] = [
            u.0, u.1, u.2, u.3, u.4, u.5, u.6, u.7,
            u.8, u.9, u.10, u.11, u.12, u.13, u.14, u.15
        ]
        let disco: [UInt8] = [
            b[3], b[2], b[1], b[0],
            b[5], b[4], b[7], b[6],
            b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]
        ]
        datos.replaceSubrange(offset..<(offset + 16), with: disco)
    }

    private func escribirUInt32LE(_ valor: UInt32, en datos: inout Data, offset: Int) {
        datos[offset] = UInt8(valor & 0xFF)
        datos[offset + 1] = UInt8((valor >> 8) & 0xFF)
        datos[offset + 2] = UInt8((valor >> 16) & 0xFF)
        datos[offset + 3] = UInt8((valor >> 24) & 0xFF)
    }
}
