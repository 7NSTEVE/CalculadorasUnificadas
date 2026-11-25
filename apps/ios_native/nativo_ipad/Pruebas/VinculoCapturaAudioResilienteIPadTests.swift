import XCTest

final class VinculoCapturaAudioResilienteIPadTests: XCTestCase {
    private var raiz: URL!
    private var baseAudio: URL!
    private var baseVinculos: URL!

    override func setUp() {
        super.setUp()
        raiz = FileManager.default.temporaryDirectory.appendingPathComponent(
            "CuadernU-VinculoAudio-\(UUID().uuidString)", isDirectory: true)
        baseAudio = raiz.appendingPathComponent("audio", isDirectory: true)
        baseVinculos = raiz.appendingPathComponent("vinculos", isDirectory: true)
        try? FileManager.default.createDirectory(
            at: raiz, withIntermediateDirectories: true)
    }

    override func tearDown() {
        if let raiz { try? FileManager.default.removeItem(at: raiz) }
        super.tearDown()
    }

    private func repositorios() -> (
        RepositorioAudioResilienteIPad,
        RepositorioVinculosCapturaAudioIPad
    ) {
        let audio = RepositorioAudioResilienteIPad(base: baseAudio)
        let vinculos = RepositorioVinculosCapturaAudioIPad(
            base: baseVinculos, raizContenedor: raiz)
        return (audio, vinculos)
    }

    func testFuenteDentroDelContenedorSePersisteRelativaYSeReconstruye() throws {
        let (audio, vinculos) = repositorios()
        let vinculo = try vinculos.crear(
            documentoId: 42, modo: .soloAudio, repositorioAudio: audio)
        let fuente = raiz.appendingPathComponent("Documentos/clase.m4a")
        try FileManager.default.createDirectory(
            at: fuente.deletingLastPathComponent(),
            withIntermediateDirectories: true)
        try Data("audio".utf8).write(to: fuente)

        let actualizado = try vinculos.vincularFuente(
            fuente, sesion: vinculo.id, esManifiestoNotaViva: false)

        XCTAssertEqual(actualizado.estado, .capturando)
        XCTAssertFalse(actualizado.fuenteRelativa?.hasPrefix("/") ?? true)
        XCTAssertEqual(vinculos.fuente(actualizado)?.standardizedFileURL,
                       fuente.standardizedFileURL)
    }

    func testFuenteFueraDelContenedorSeRechaza() throws {
        let (audio, vinculos) = repositorios()
        let vinculo = try vinculos.crear(
            documentoId: 7, modo: .soloAudio, repositorioAudio: audio)
        let externa = FileManager.default.temporaryDirectory
            .appendingPathComponent("externa-\(UUID().uuidString).m4a")
        defer { try? FileManager.default.removeItem(at: externa) }
        try Data("audio".utf8).write(to: externa)

        XCTAssertThrowsError(try vinculos.vincularFuente(
            externa, sesion: vinculo.id, esManifiestoNotaViva: false)) { error in
            XCTAssertEqual(error as? ErrorVinculoCapturaAudioIPad,
                           .rutaFueraDelContenedor)
        }
    }

    func testStartupMarcaPreparandoCapturandoYPausadoPeroNoTerminales() throws {
        let (audio, vinculos) = repositorios()
        let preparando = try vinculos.crear(
            documentoId: 1, modo: .soloAudio, repositorioAudio: audio)
        let capturando = try vinculos.crear(
            documentoId: 2, modo: .soloAudio, repositorioAudio: audio)
        let pausado = try vinculos.crear(
            documentoId: 3, modo: .notaViva, repositorioAudio: audio)
        let confirmado = try vinculos.crear(
            documentoId: 4, modo: .soloAudio, repositorioAudio: audio)
        let cancelado = try vinculos.crear(
            documentoId: 5, modo: .soloAudio, repositorioAudio: audio)

        let fuente = raiz.appendingPathComponent("captura.m4a")
        try Data("bytes".utf8).write(to: fuente)
        _ = try vinculos.vincularFuente(
            fuente, sesion: capturando.id, esManifiestoNotaViva: false)
        _ = try vinculos.vincularFuente(
            fuente, sesion: pausado.id, esManifiestoNotaViva: true)
        _ = try vinculos.marcarPausa(pausado.id)
        _ = try vinculos.marcarConfirmado(confirmado.id)
        _ = try vinculos.marcarCancelado(cancelado.id, motivo: "permiso denegado")

        let marcados = Set(try vinculos.marcarInterrumpidosParaRecovery())
        XCTAssertEqual(marcados, Set([preparando.id, capturando.id, pausado.id]))
        XCTAssertEqual(try vinculos.leer(confirmado.id).estado, .confirmado)
        XCTAssertEqual(try vinculos.leer(cancelado.id).estado, .cancelado)
    }

    func testIntegradorConfirmaAudioPrincipalSinModificarOriginal() throws {
        let (audio, vinculos) = repositorios()
        let integrador = IntegradorAudioResilienteGrabacionIPad(
            audio: audio, vinculos: vinculos)
        XCTAssertNil(integrador.preparar(
            documentoId: 99, modo: .soloAudio, microfono: true))
        guard let id = integrador.activo?.sesionAudioId else {
            return XCTFail("Debe existir sesión resiliente")
        }

        let original = raiz.appendingPathComponent("clase-final.m4a")
        let bytes = Data(repeating: 0x5A, count: 8_192)
        try bytes.write(to: original)
        XCTAssertNil(integrador.vincularFuente(original, modo: .soloAudio))
        XCTAssertNil(integrador.pausar())
        XCTAssertNil(integrador.reanudar())

        var ajustes = TintaAjustesGrabacion.porDefecto
        ajustes.microfono = true
        ajustes.frecuencia_hz = 48_000
        XCTAssertNil(integrador.confirmarFinal(
            resultadoPrincipal: original,
            modo: .soloAudio,
            duracion: 12.5,
            ajustes: ajustes))

        XCTAssertEqual(try Data(contentsOf: original), bytes)
        let manifest = try audio.leer(id)
        XCTAssertEqual(manifest.estado, .finalizada)
        XCTAssertEqual(manifest.segmentos.count, 1)
        XCTAssertEqual(manifest.segmentos[0].duracionSegundos, 12.5,
                       accuracy: 0.0001)
        XCTAssertEqual(try vinculos.leer(id).estado, .confirmado)
    }

    func testResolverNotaVivaUsaSoloArchivoAudioDeclarado() throws {
        let paquete = raiz.appendingPathComponent("Clase.tintaviva", isDirectory: true)
        try FileManager.default.createDirectory(
            at: paquete, withIntermediateDirectories: true)
        let audio = paquete.appendingPathComponent("audio-final.m4a")
        try Data("audio-final".utf8).write(to: audio)
        let manifestURL = paquete.appendingPathComponent("manifest.json")

        let manifest = ManifiestoNotaViva(
            version: ManifiestoNotaViva.versionActual,
            id: UUID(),
            creado: Date(),
            inicioMonotono: 0,
            duracion: 3,
            archivoTinta: "nota.tinta",
            archivoCronologia: nil,
            documentoPersistenteId: nil,
            archivoAudio: audio.lastPathComponent,
            archivoAudioParcial: nil,
            advertencias: nil,
            archivoAudioMejorado: nil,
            ajustesAudioMejorado: nil,
            silencios: nil,
            archivoVideoAutorizado: nil,
            videoAutorizadoConfirmado: nil,
            bitrateAudioKbps: 128,
            bitrateAudioAdministradoPorSistema: nil,
            audioEspacialFOA: nil,
            frecuenciaHz: 48_000,
            canales: 1,
            microfono: nil,
            segmentos: [],
            eventos: [],
            archivoTranscripcion: nil,
            transcripcion: nil,
            capitulos: nil)
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        try encoder.encode(manifest).write(to: manifestURL)

        XCTAssertEqual(
            try ResolutorAudioFinalNotaVivaIPad.resolver(
                manifiestoURL: manifestURL)?.standardizedFileURL,
            audio.standardizedFileURL)
    }
}
