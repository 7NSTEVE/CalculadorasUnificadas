import XCTest
@testable import CuadernUEstudio

final class CicloVidaIntegridadDocumentoIPadTests: XCTestCase {
    private func base() throws -> URL {
        let u = FileManager.default.temporaryDirectory
            .appendingPathComponent("ciclo-integridad-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: u, withIntermediateDirectories: true)
        return u
    }

    private func sistema(_ raiz: URL, proceso: UUID = UUID()) -> (
        coordinador: CoordinadorCicloVidaIntegridadIPad,
        sesiones: RepositorioSesionesPersistenciaDocumentoIPad,
        revisiones: RepositorioRevisionesGuardadoIPad,
        tx: CoordinadorPersistenciaTransaccionalIPad
    ) {
        let txBase = raiz.appendingPathComponent("tx", isDirectory: true)
        let tx = CoordinadorPersistenciaTransaccionalIPad(base: txBase)
        let sesiones = RepositorioSesionesPersistenciaDocumentoIPad(
            base: raiz.appendingPathComponent("sesiones"), procesoId: proceso)
        let revisiones = RepositorioRevisionesGuardadoIPad(
            base: raiz.appendingPathComponent("revisiones"))
        let coordinador = CoordinadorCicloVidaIntegridadIPad(
            transacciones: tx,
            sesiones: sesiones,
            revisiones: revisiones,
            baseTransacciones: txBase)
        return (coordinador, sesiones, revisiones, tx)
    }

    func testPrimeraAperturaSinManifestEsPermitida() throws {
        let s = sistema(try base())
        let doc = DocumentoPersistenteID()
        XCTAssertEqual(s.coordinador.evaluarApertura(doc), .abrir)
        _ = try s.coordinador.abrir(doc, escenaId: "A")
        try s.coordinador.cerrar(doc, escenaId: "A")
    }

    func testSesionStaleExigeAceptacionAntesDeAbrir() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let procesoA = sistema(raiz, proceso: UUID())
        _ = try procesoA.coordinador.abrir(doc, escenaId: "A")
        // No cerrar: crash.
        let procesoB = sistema(raiz, proceso: UUID())
        guard case .requiereAceptarRecovery = procesoB.coordinador.evaluarApertura(doc) else {
            return XCTFail("Debió exigir recovery")
        }
        XCTAssertThrowsError(try procesoB.coordinador.abrir(doc, escenaId: "B"))
        try procesoB.coordinador.aceptarRecoveryYHabilitarApertura(doc)
        XCTAssertEqual(procesoB.coordinador.evaluarApertura(doc), .abrir)
    }

    func testManifestCorruptoBloqueaAunqueNoHayaSesionStale() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let s = sistema(raiz)
        try FileManager.default.createDirectory(
            at: s.revisiones.url(doc).deletingLastPathComponent(),
            withIntermediateDirectories: true)
        try Data("basura".utf8).write(to: s.revisiones.url(doc))
        guard case .bloquearManifest = s.coordinador.evaluarApertura(doc) else {
            return XCTFail("Debió bloquear manifest")
        }
    }

    func testContenidoAlteradoBloqueaApertura() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let s = sistema(raiz)
        let archivo = raiz.appendingPathComponent("doc")
        let prep = try s.revisiones.preparar(
            documentoId: doc,
            cambios: [.init(destino: archivo,
                            mutacion: .escribir(Data("ok".utf8)))])
        try Data("ok".utf8).write(to: archivo)
        if case .escribir(let manifest) = prep.cambioManifest.mutacion {
            try manifest.write(to: s.revisiones.url(doc))
        }
        try Data("alterado".utf8).write(to: archivo)
        guard case .bloquearContenido(let reporte) = s.coordinador.evaluarApertura(doc) else {
            return XCTFail("Debió bloquear contenido")
        }
        XCTAssertFalse(reporte.integra)
    }

    func testArranqueRecuperaJournalAntesDeRestaurarBiblioteca() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let s = sistema(raiz)
        let archivo = raiz.appendingPathComponent("doc")
        try Data("v1".utf8).write(to: archivo)
        let p = try s.tx.preparar(
            documentoId: doc,
            cambios: [.init(destino: archivo,
                            mutacion: .escribir(Data("v2".utf8)))])
        try Data("v2".utf8).write(to: archivo)

        let reporte = s.coordinador.prepararArranque()
        XCTAssertTrue(reporte.permiteRestaurarBiblioteca)
        guard case .recuperado(let cantidad) = reporte.recovery else {
            return XCTFail("Debió recuperar el journal")
        }
        XCTAssertEqual(cantidad, 1)
        XCTAssertEqual(try Data(contentsOf: archivo), Data("v1".utf8))
        XCTAssertThrowsError(try s.tx.journal(p.id))
    }
}
