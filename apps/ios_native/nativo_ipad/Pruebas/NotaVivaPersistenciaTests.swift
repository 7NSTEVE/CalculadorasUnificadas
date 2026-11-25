import XCTest
@testable import CuadernUEstudio

final class NotaVivaPersistenciaTests: XCTestCase {
    private var carpeta: URL!
    private var manifiestoURL: URL!

    override func setUpWithError() throws {
        carpeta = FileManager.default.temporaryDirectory
            .appendingPathComponent("NotaVivaPersistenciaTests-\(UUID().uuidString)",
                                    isDirectory: true)
        try FileManager.default.createDirectory(at: carpeta,
                                                withIntermediateDirectories: false)
        manifiestoURL = carpeta.appendingPathComponent("manifest.json")
    }

    override func tearDownWithError() throws {
        if let carpeta {
            try? FileManager.default.removeItem(at: carpeta)
        }
        carpeta = nil
        manifiestoURL = nil
    }

    func testSegundaEscrituraConservaLaVersionAnteriorValida() throws {
        var primero = manifiesto(duracion: 10)
        try RepositorioManifiestoNotaViva.guardar(primero, en: manifiestoURL)

        primero.duracion = 20
        try RepositorioManifiestoNotaViva.guardar(primero, en: manifiestoURL)

        try Data("{corrupto".utf8).write(to: manifiestoURL, options: .atomic)
        let recuperado = try RepositorioManifiestoNotaViva.cargar(manifiestoURL)

        XCTAssertEqual(recuperado.duracion, 10, accuracy: 0.000_001)
        XCTAssertTrue(tieneAvisoDeRecuperacion(recuperado))
    }

    func testCargaUsaRespaldoSiElPrincipalQuedaCorrupto() throws {
        var valor = manifiesto(duracion: 12)
        try RepositorioManifiestoNotaViva.guardar(valor, en: manifiestoURL)
        valor.duracion = 24
        try RepositorioManifiestoNotaViva.guardar(valor, en: manifiestoURL)

        try Data([0xFF, 0x00, 0x7B]).write(to: manifiestoURL, options: .atomic)

        let recuperado = try RepositorioManifiestoNotaViva.cargar(manifiestoURL)
        XCTAssertEqual(recuperado.duracion, 12, accuracy: 0.000_001)
        XCTAssertTrue(tieneAvisoDeRecuperacion(recuperado))
    }

    func testCheckpointSeUsaSiNoExisteManifiestoFinal() throws {
        let checkpoint = manifiesto(duracion: 3)
        try RepositorioManifiestoNotaViva.guardarCheckpoint(
            checkpoint, en: manifiestoURL)

        let recuperado = try RepositorioManifiestoNotaViva.cargar(manifiestoURL)

        XCTAssertEqual(recuperado.duracion, 3, accuracy: 0.000_001)
        XCTAssertTrue(tieneAvisoCheckpoint(recuperado))
    }

    func testPublicarManifiestoFinalEliminaCheckpoint() throws {
        let checkpoint = manifiesto(duracion: 1)
        try RepositorioManifiestoNotaViva.guardarCheckpoint(
            checkpoint, en: manifiestoURL)
        let checkpointURL = carpeta.appendingPathComponent("manifest.checkpoint.json")
        XCTAssertTrue(FileManager.default.fileExists(atPath: checkpointURL.path))

        let final = manifiesto(duracion: 22)
        try RepositorioManifiestoNotaViva.guardar(final, en: manifiestoURL)

        XCTAssertFalse(FileManager.default.fileExists(atPath: checkpointURL.path))
        let recargado = try RepositorioManifiestoNotaViva.cargar(manifiestoURL)
        XCTAssertEqual(recargado.duracion, 22, accuracy: 0.000_001)
        XCTAssertFalse(tieneAvisoCheckpoint(recargado))
    }

    func testRespaldoCorruptoNoImpideRecuperarCheckpoint() throws {
        let checkpoint = manifiesto(duracion: 7)
        try RepositorioManifiestoNotaViva.guardarCheckpoint(
            checkpoint, en: manifiestoURL)
        try Data("respaldo roto".utf8).write(
            to: carpeta.appendingPathComponent("manifest.previous.json"),
            options: .atomic)

        let recuperado = try RepositorioManifiestoNotaViva.cargar(manifiestoURL)
        XCTAssertEqual(recuperado.duracion, 7, accuracy: 0.000_001)
        XCTAssertTrue(tieneAvisoCheckpoint(recuperado))
    }

    func testActualizarTrasCorrupcionReconstruyeUnPrincipalLegible() throws {
        var valor = manifiesto(duracion: 5)
        try RepositorioManifiestoNotaViva.guardar(valor, en: manifiestoURL)
        valor.duracion = 9
        try RepositorioManifiestoNotaViva.guardar(valor, en: manifiestoURL)

        try Data("manifiesto roto".utf8).write(to: manifiestoURL,
                                               options: .atomic)

        let actualizado = try RepositorioManifiestoNotaViva.actualizar(manifiestoURL) {
            $0.duracion = 30
            $0.advertencias = ($0.advertencias ?? []) + ["actualizado"]
        }

        XCTAssertEqual(actualizado.duracion, 30, accuracy: 0.000_001)
        XCTAssertTrue(tieneAvisoDeRecuperacion(actualizado))
        XCTAssertTrue(actualizado.advertencias?.contains("actualizado") == true)

        let recargado = try RepositorioManifiestoNotaViva.cargar(manifiestoURL)
        XCTAssertEqual(recargado.duracion, 30, accuracy: 0.000_001)
        XCTAssertTrue(tieneAvisoDeRecuperacion(recargado))
        XCTAssertTrue(recargado.advertencias?.contains("actualizado") == true)
    }

    func testSinPrincipalNiRespaldoNiCheckpointValidoLaCargaFalla() throws {
        try Data("invalido".utf8).write(to: manifiestoURL, options: .atomic)
        XCTAssertThrowsError(
            try RepositorioManifiestoNotaViva.cargar(manifiestoURL))
    }

    private func tieneAvisoDeRecuperacion(_ manifiesto: ManifiestoNotaViva) -> Bool {
        manifiesto.advertencias?.contains {
            $0.contains("última versión válida")
        } == true
    }

    private func tieneAvisoCheckpoint(_ manifiesto: ManifiestoNotaViva) -> Bool {
        manifiesto.advertencias?.contains {
            $0.contains("checkpoint de recuperación")
        } == true
    }

    private func manifiesto(duracion: Double) -> ManifiestoNotaViva {
        ManifiestoNotaViva(
            version: ManifiestoNotaViva.versionActual,
            id: UUID(),
            creado: Date(timeIntervalSince1970: 1_700_000_000),
            inicioMonotono: 100,
            duracion: duracion,
            archivoTinta: "nota.tinta",
            archivoCronologia: "cronologia.tintalive",
            documentoPersistenteId: UUID(),
            archivoAudio: "audio.m4a",
            archivoAudioParcial: nil,
            advertencias: nil,
            archivoAudioMejorado: nil,
            ajustesAudioMejorado: nil,
            silencios: nil,
            archivoVideoAutorizado: nil,
            videoAutorizadoConfirmado: nil,
            bitrateAudioKbps: 128,
            bitrateAudioAdministradoPorSistema: false,
            audioEspacialFOA: false,
            frecuenciaHz: 48_000,
            canales: 1,
            microfono: nil,
            segmentos: [
                SegmentoRelojNotaViva(
                    segundoInicio: 0,
                    monotonoInicio: 100,
                    monotonoFin: 100 + duracion)
            ],
            eventos: [
                EventoNotaViva(tipo: .inicio, segundo: 0),
                EventoNotaViva(tipo: .fin, segundo: duracion)
            ],
            archivoTranscripcion: nil,
            transcripcion: nil,
            capitulos: nil)
    }
}