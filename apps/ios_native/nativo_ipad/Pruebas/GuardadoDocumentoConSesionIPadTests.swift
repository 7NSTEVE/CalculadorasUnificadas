import XCTest
@testable import CuadernUEstudio

private final class ParticipanteSesionGuardadoPruebaIPad: ParticipanteGuardadoDocumentoIPad {
    let nombreParticipanteGuardado = "doc"
    let destino: URL
    init(destino: URL) { self.destino = destino }
    func cambiosParaGuardar(documentoId: DocumentoPersistenteID) throws
        -> [CambioArchivoTransaccionalIPad] {
        [.init(destino: destino,
               mutacion: .escribir(Data("guardado".utf8)),
               precondicion: .debeNoExistir)]
    }
}

final class GuardadoDocumentoConSesionIPadTests: XCTestCase {
    private func base() throws -> URL {
        let u = FileManager.default.temporaryDirectory
            .appendingPathComponent("save-session-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: u, withIntermediateDirectories: true)
        return u
    }

    func testRevisionConfirmadaSeRegistraEnSesion() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let sesiones = RepositorioSesionesPersistenciaDocumentoIPad(
            base: raiz.appendingPathComponent("sesiones"), procesoId: UUID())
        _ = try sesiones.abrir(doc, escenaId: "A")
        let guardado = CoordinadorGuardadoDocumentoAtomicoIPad(
            transacciones: .init(base: raiz.appendingPathComponent("journals")),
            revisiones: .init(base: raiz.appendingPathComponent("revisiones")))
        let resultado = try CoordinadorGuardadoConSesionIPad(
            guardado: guardado, sesiones: sesiones).guardar(
                documentoId: doc,
                participantes: [ParticipanteSesionGuardadoPruebaIPad(
                    destino: raiz.appendingPathComponent("doc"))])

        XCTAssertTrue(resultado.guardado.integridadVerificada)
        XCTAssertTrue(resultado.revisionRegistradaEnSesion)
        XCTAssertNil(resultado.advertenciaSesion)
        guard case .yaAbiertoEnEsteProceso = try sesiones.evaluarAntesDeAbrir(doc) else {
            return XCTFail("La sesión debería seguir abierta")
        }

        try sesiones.cerrar(doc, escenaId: "A")
        XCTAssertEqual(
            try sesiones.evaluarAntesDeAbrir(doc),
            .limpio(ultimaRevisionId: resultado.guardado.revisionId))
    }

    func testFalloDelMarcadorNoConvierteCommitExitosoEnFalloDeDatos() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let baseInvalida = raiz.appendingPathComponent("sesiones-como-archivo")
        try Data("no-directorio".utf8).write(to: baseInvalida)
        let sesiones = RepositorioSesionesPersistenciaDocumentoIPad(
            base: baseInvalida, procesoId: UUID())
        let destino = raiz.appendingPathComponent("doc")
        let guardado = CoordinadorGuardadoDocumentoAtomicoIPad(
            transacciones: .init(base: raiz.appendingPathComponent("journals")),
            revisiones: .init(base: raiz.appendingPathComponent("revisiones")))

        let resultado = try CoordinadorGuardadoConSesionIPad(
            guardado: guardado, sesiones: sesiones).guardar(
                documentoId: doc,
                participantes: [ParticipanteSesionGuardadoPruebaIPad(destino: destino)])

        XCTAssertTrue(resultado.guardado.integridadVerificada)
        XCTAssertFalse(resultado.revisionRegistradaEnSesion)
        XCTAssertNotNil(resultado.advertenciaSesion)
        XCTAssertEqual(try Data(contentsOf: destino), Data("guardado".utf8))
    }
}
