import XCTest
@testable import CuadernUEstudio

final class SesionesPersistenciaDocumentoIPadTests: XCTestCase {
    private func base() throws -> URL {
        let u = FileManager.default.temporaryDirectory
            .appendingPathComponent("sesion-doc-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: u, withIntermediateDirectories: true)
        return u
    }

    func testDosEscenasMismoProcesoNoParecenCrash() throws {
        let raiz = try base(), proceso = UUID(), doc = DocumentoPersistenteID()
        let repo = RepositorioSesionesPersistenciaDocumentoIPad(
            base: raiz, procesoId: proceso)
        XCTAssertEqual(try repo.evaluarAntesDeAbrir(doc), .primeraApertura)
        _ = try repo.abrir(doc, escenaId: "scene-A")
        _ = try repo.abrir(doc, escenaId: "scene-B")
        XCTAssertEqual(
            try repo.evaluarAntesDeAbrir(doc),
            .yaAbiertoEnEsteProceso(escenas: ["scene-A", "scene-B"]))
    }

    func testNuevoProcesoDetectaCierreNoLimpio() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let revision = UUID()
        let procesoA = RepositorioSesionesPersistenciaDocumentoIPad(
            base: raiz, procesoId: UUID())
        try procesoA.registrarRevision(doc, revisionId: revision)
        _ = try procesoA.abrir(doc, escenaId: "principal")
        // No cerrar: simula crash/force-quit.

        let procesoB = RepositorioSesionesPersistenciaDocumentoIPad(
            base: raiz, procesoId: UUID())
        XCTAssertEqual(
            try procesoB.evaluarAntesDeAbrir(doc),
            .requiereRecovery(aperturasStale: 1, ultimaRevisionId: revision))
        XCTAssertThrowsError(try procesoB.abrir(doc, escenaId: "nueva")) {
            XCTAssertEqual($0 as? ErrorSesionPersistenciaDocumentoIPad,
                           .recoveryPendiente)
        }
    }

    func testAceptarRecoveryLimpiaStaleYPermiteAbrir() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let a = RepositorioSesionesPersistenciaDocumentoIPad(
            base: raiz, procesoId: UUID())
        _ = try a.abrir(doc, escenaId: "vieja")

        let b = RepositorioSesionesPersistenciaDocumentoIPad(
            base: raiz, procesoId: UUID())
        try b.aceptarRecovery(doc)
        XCTAssertEqual(try b.evaluarAntesDeAbrir(doc), .limpio(ultimaRevisionId: nil))
        _ = try b.abrir(doc, escenaId: "nueva")
        XCTAssertEqual(try b.evaluarAntesDeAbrir(doc),
                       .yaAbiertoEnEsteProceso(escenas: ["nueva"]))
    }

    func testCierreDeUltimaEscenaDejaSesionLimpia() throws {
        let raiz = try base(), proceso = UUID(), doc = DocumentoPersistenteID()
        let repo = RepositorioSesionesPersistenciaDocumentoIPad(
            base: raiz, procesoId: proceso)
        _ = try repo.abrir(doc, escenaId: "A")
        _ = try repo.abrir(doc, escenaId: "B")
        try repo.cerrar(doc, escenaId: "A")
        XCTAssertEqual(try repo.evaluarAntesDeAbrir(doc),
                       .yaAbiertoEnEsteProceso(escenas: ["B"]))
        try repo.cerrar(doc, escenaId: "B")
        XCTAssertEqual(try repo.evaluarAntesDeAbrir(doc),
                       .limpio(ultimaRevisionId: nil))
    }

    func testHeartbeatNoCambiaIdentidadDeApertura() throws {
        let raiz = try base(), proceso = UUID(), doc = DocumentoPersistenteID()
        let repo = RepositorioSesionesPersistenciaDocumentoIPad(
            base: raiz, procesoId: proceso)
        let apertura = try repo.abrir(doc, escenaId: "A")
        try repo.heartbeat(doc, escenaId: "A",
                           ahora: Date(timeIntervalSince1970: 500))
        let repetida = try repo.abrir(doc, escenaId: "A")
        XCTAssertEqual(apertura.id, repetida.id)
    }

    func testGateBloqueaManifestAlteradoAunqueSesionSeaLimpia() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let sesiones = RepositorioSesionesPersistenciaDocumentoIPad(
            base: raiz.appendingPathComponent("sesiones"), procesoId: UUID())
        let revisiones = RepositorioRevisionesGuardadoIPad(
            base: raiz.appendingPathComponent("revisiones"))
        let archivo = raiz.appendingPathComponent("doc")
        let preparacion = try revisiones.preparar(
            documentoId: doc,
            cambios: [.init(destino: archivo,
                            mutacion: .escribir(Data("correcto".utf8)))])
        try Data("correcto".utf8).write(to: archivo)
        try FileManager.default.createDirectory(
            at: revisiones.url(doc).deletingLastPathComponent(),
            withIntermediateDirectories: true)
        if case .escribir(let manifest) = preparacion.cambioManifest.mutacion {
            try manifest.write(to: revisiones.url(doc))
        }
        try sesiones.registrarRevision(doc, revisionId: preparacion.revision.revisionId)
        try Data("alterado".utf8).write(to: archivo)

        let gate = GatePreaperturaSeguraIPad(
            sesiones: sesiones,
            revisiones: GateReaperturaDocumentoIPad(revisiones: revisiones))
        XCTAssertEqual(gate.evaluar(doc).decision, .bloquearIntegridad)
    }
}
