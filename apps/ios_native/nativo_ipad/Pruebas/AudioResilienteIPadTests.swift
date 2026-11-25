import XCTest

final class AudioResilienteIPadTests: XCTestCase {
    private var carpeta: URL!
    private var repo: RepositorioAudioResilienteIPad!

    override func setUp() {
        super.setUp()
        carpeta = FileManager.default.temporaryDirectory.appendingPathComponent(
            "CuadernU-AudioResiliente-\(UUID().uuidString)",
            isDirectory: true)
        try? FileManager.default.createDirectory(
            at: carpeta, withIntermediateDirectories: true)
        repo = RepositorioAudioResilienteIPad(base: carpeta)
    }

    override func tearDown() {
        if let carpeta { try? FileManager.default.removeItem(at: carpeta) }
        repo = nil
        super.tearDown()
    }

    private func temporal(_ nombre: String, bytes: String) throws -> URL {
        let url = FileManager.default.temporaryDirectory.appendingPathComponent(
            "\(UUID().uuidString)-\(nombre)")
        try Data(bytes.utf8).write(to: url, options: .atomic)
        addTeardownBlock { try? FileManager.default.removeItem(at: url) }
        return url
    }

    func testConfirmarSegmentoPublicaBytesAntesDeManifestDurable() throws {
        let sesion = try repo.crearSesion(documentoId: 42)
        let origen = try temporal("uno.m4a", bytes: "audio-segmento-uno")
        let segmento = try repo.confirmarSegmento(
            sesion: sesion.id,
            archivoTemporal: origen,
            inicioSegundos: 0,
            duracionSegundos: 10,
            codec: "AAC-LC",
            extensionArchivo: "m4a",
            sampleRate: 48_000,
            canales: 2)

        let recargado = try repo.leer(sesion.id)
        XCTAssertEqual(recargado.segmentos, [segmento])
        XCTAssertEqual(recargado.duracionConfirmada, 10, accuracy: 0.001)
        XCTAssertEqual(segmento.sha256.count, 64)
        XCTAssertGreaterThan(segmento.bytes, 0)
        let diagnostico = try repo.diagnosticar(sesion.id)
        XCTAssertTrue(diagnostico.integro)
        XCTAssertTrue(diagnostico.archivosHuerfanos.isEmpty)
    }

    func testMismoContenidoNoSeConfirmaDosVeces() throws {
        let sesion = try repo.crearSesion(documentoId: 1)
        let origen = try temporal("duplicado.m4a", bytes: "mismo-audio")
        _ = try repo.confirmarSegmento(
            sesion: sesion.id,
            archivoTemporal: origen,
            inicioSegundos: 0,
            duracionSegundos: 5,
            codec: "AAC",
            extensionArchivo: "m4a")
        XCTAssertThrowsError(try repo.confirmarSegmento(
            sesion: sesion.id,
            archivoTemporal: origen,
            inicioSegundos: 5,
            duracionSegundos: 5,
            codec: "AAC",
            extensionArchivo: "m4a")) { error in
                XCTAssertEqual(error as? ErrorAudioResilienteIPad, .segmentoDuplicado)
            }
        XCTAssertEqual(try repo.leer(sesion.id).segmentos.count, 1)
    }

    func testPausaNoSeMarcaComoCrashPeroGrabandoSi() throws {
        let grabando = try repo.crearSesion(documentoId: 10)
        let pausada = try repo.crearSesion(documentoId: 10)
        try repo.pausar(pausada.id)

        let afectadas = try repo.marcarInterrumpidasParaRecovery()
        XCTAssertTrue(afectadas.contains(grabando.id))
        XCTAssertFalse(afectadas.contains(pausada.id))
        XCTAssertEqual(try repo.leer(grabando.id).estado, .requiereRecuperacion)
        XCTAssertEqual(try repo.leer(pausada.id).estado, .pausada)
    }

    func testArchivoAlteradoSeDetectaYBloqueaFinalizacion() throws {
        let sesion = try repo.crearSesion(documentoId: 8)
        let origen = try temporal("tamper.m4a", bytes: "audio-original")
        let segmento = try repo.confirmarSegmento(
            sesion: sesion.id,
            archivoTemporal: origen,
            inicioSegundos: 0,
            duracionSegundos: 2,
            codec: "AAC",
            extensionArchivo: "m4a")

        let ruta = carpeta.appendingPathComponent(sesion.id.uuidString)
            .appendingPathComponent(segmento.archivo)
        try Data("audio-alterado".utf8).write(to: ruta, options: .atomic)
        let diagnostico = try repo.diagnosticar(sesion.id)
        XCTAssertEqual(diagnostico.segmentosAlterados, [segmento.archivo])
        XCTAssertThrowsError(try repo.finalizar(sesion.id)) { error in
            XCTAssertEqual(error as? ErrorAudioResilienteIPad, .sesionInconsistente)
        }
    }

    func testArchivoHuerfanoSePreservaYNoSeAdjuntaAutomaticamente() throws {
        let sesion = try repo.crearSesion(documentoId: 11)
        let carpetaSesion = carpeta.appendingPathComponent(sesion.id.uuidString)
        let huerfano = carpetaSesion.appendingPathComponent("segmento-000999.m4a")
        try Data("bytes-posible-recovery".utf8).write(to: huerfano, options: .atomic)

        let diagnostico = try repo.diagnosticar(sesion.id)
        XCTAssertEqual(diagnostico.archivosHuerfanos, [huerfano.lastPathComponent])
        XCTAssertTrue(try repo.leer(sesion.id).segmentos.isEmpty)
        XCTAssertTrue(FileManager.default.fileExists(atPath: huerfano.path))
        XCTAssertThrowsError(try repo.finalizar(sesion.id))
        XCTAssertTrue(FileManager.default.fileExists(atPath: huerfano.path))
    }

    func testSegmentoFaltanteSeDetectaSinRecrearArchivoVacio() throws {
        let sesion = try repo.crearSesion(documentoId: 15)
        let origen = try temporal("missing.m4a", bytes: "audio")
        let segmento = try repo.confirmarSegmento(
            sesion: sesion.id,
            archivoTemporal: origen,
            inicioSegundos: 0,
            duracionSegundos: 1,
            codec: "AAC",
            extensionArchivo: "m4a")
        let ruta = carpeta.appendingPathComponent(sesion.id.uuidString)
            .appendingPathComponent(segmento.archivo)
        try FileManager.default.removeItem(at: ruta)

        let diagnostico = try repo.diagnosticar(sesion.id)
        XCTAssertEqual(diagnostico.segmentosFaltantes, [segmento.archivo])
        XCTAssertFalse(FileManager.default.fileExists(atPath: ruta.path))
        XCTAssertThrowsError(try repo.finalizar(sesion.id))
        XCTAssertFalse(FileManager.default.fileExists(atPath: ruta.path))
    }

    func testFinalizadaNoAceptaNuevosSegmentos() throws {
        let sesion = try repo.crearSesion(documentoId: 77)
        let origen = try temporal("final.m4a", bytes: "audio-final")
        _ = try repo.confirmarSegmento(
            sesion: sesion.id,
            archivoTemporal: origen,
            inicioSegundos: 0,
            duracionSegundos: 3,
            codec: "AAC",
            extensionArchivo: "m4a")
        try repo.finalizar(sesion.id)
        XCTAssertEqual(try repo.leer(sesion.id).estado, .finalizada)
        XCTAssertThrowsError(try repo.confirmarSegmento(
            sesion: sesion.id,
            archivoTemporal: origen,
            inicioSegundos: 3,
            duracionSegundos: 3,
            codec: "AAC",
            extensionArchivo: "m4a")) { error in
                XCTAssertEqual(error as? ErrorAudioResilienteIPad, .sesionFinalizada)
            }
    }

    func testListadoFiltraPorDocumento() throws {
        let a = try repo.crearSesion(documentoId: 100)
        _ = try repo.crearSesion(documentoId: 200)
        let filtradas = repo.sesiones(documentoId: 100)
        XCTAssertEqual(filtradas.map(\.id), [a.id])
        XCTAssertEqual(repo.sesiones().count, 2)
    }
}
