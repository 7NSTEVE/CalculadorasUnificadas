import XCTest
@testable import CuadernUEstudio

final class EstadoEscenasYStageManagerIPadTests: XCTestCase {
    private func base() throws -> URL {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("escenas-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        return url
    }

    func testPoliticaSeAdaptaATresAnchos() {
        XCTAssertEqual(PoliticaDisposicionCuadernUIPad.calcular(
            tamano: CGSize(width: 500, height: 900)).claseAncho, .compacta)
        XCTAssertEqual(PoliticaDisposicionCuadernUIPad.calcular(
            tamano: CGSize(width: 800, height: 900)).claseAncho, .media)
        XCTAssertEqual(PoliticaDisposicionCuadernUIPad.calcular(
            tamano: CGSize(width: 1200, height: 900)).claseAncho, .expandida)
    }

    func testTecladoReduceColumnasSinCambiarIdentidad() {
        let normal = PoliticaDisposicionCuadernUIPad.calcular(
            tamano: CGSize(width: 850, height: 700), tecladoVisible: false)
        let teclado = PoliticaDisposicionCuadernUIPad.calcular(
            tamano: CGSize(width: 850, height: 700), tecladoVisible: true)
        XCTAssertEqual(normal.claseAncho, teclado.claseAncho)
        XCTAssertGreaterThan(normal.columnas, teclado.columnas)
        XCTAssertFalse(teclado.permitePanelLateralFijo)
    }

    func testEscenasNoCruzanDocumentoNiViewport() throws {
        let base = try base()
        let repo = RepositorioEstadoEscenasCuadernUIPad(base: base)
        let docA = DocumentoPersistenteID(), docB = DocumentoPersistenteID()
        var a = EstadoEscenaCuadernUIPad(escenaId: "A", documentoId: docA,
                                         paginaVisible: 3, escalaLienzo: 2)
        var b = EstadoEscenaCuadernUIPad(escenaId: "B", documentoId: docB,
                                         paginaVisible: 8, escalaLienzo: 0.5)
        a.panelLateral = .busqueda; b.panelLateral = .estudio
        a.cuadernoId = "cuaderno-A"; b.cuadernoId = "cuaderno-B"
        a.referenciaVisible = true; b.referenciaVisible = false
        XCTAssertTrue(repo.guardar(a)); XCTAssertTrue(repo.guardar(b))
        let reabierto = RepositorioEstadoEscenasCuadernUIPad(base: base)
        XCTAssertEqual(reabierto.estado(escenaId: "A").documentoId, docA)
        XCTAssertEqual(reabierto.estado(escenaId: "A").paginaVisible, 3)
        XCTAssertEqual(reabierto.estado(escenaId: "A").cuadernoId, "cuaderno-A")
        XCTAssertEqual(reabierto.estado(escenaId: "A").referenciaVisible, true)
        XCTAssertEqual(reabierto.estado(escenaId: "B").documentoId, docB)
        XCTAssertEqual(reabierto.estado(escenaId: "B").panelLateral, .estudio)
        XCTAssertEqual(reabierto.estado(escenaId: "B").cuadernoId, "cuaderno-B")
        XCTAssertEqual(reabierto.estado(escenaId: "B").referenciaVisible, false)
    }

    func testNormalizaZoomYPaginaInvalidos() throws {
        let repo = RepositorioEstadoEscenasCuadernUIPad(base: try base())
        var estado = EstadoEscenaCuadernUIPad(escenaId: "X")
        estado.paginaVisible = -10; estado.escalaLienzo = .infinity
        XCTAssertTrue(repo.guardar(estado))
        let guardado = repo.estado(escenaId: "X")
        XCTAssertEqual(guardado.paginaVisible, 0)
        XCTAssertEqual(guardado.escalaLienzo, 1)
    }

    func testActualizarMutaSoloLaEscenaObjetivo() throws {
        let repo = RepositorioEstadoEscenasCuadernUIPad(base: try base())
        var a = EstadoEscenaCuadernUIPad(escenaId: "A")
        a.cuadernoId = "uno"
        var b = EstadoEscenaCuadernUIPad(escenaId: "B")
        b.cuadernoId = "dos"
        XCTAssertTrue(repo.guardar(a)); XCTAssertTrue(repo.guardar(b))

        let pestana = UUID()
        XCTAssertTrue(repo.actualizar(escenaId: "A") { estado in
            estado.cuadernoId = "uno-nuevo"
            estado.pestanaSeleccionadaId = pestana
            estado.documentoId = DocumentoPersistenteID(pestana)
            estado.referenciaVisible = true
        })

        XCTAssertEqual(repo.estado(escenaId: "A").cuadernoId, "uno-nuevo")
        XCTAssertEqual(repo.estado(escenaId: "A").pestanaSeleccionadaId, pestana)
        XCTAssertEqual(repo.estado(escenaId: "A").referenciaVisible, true)
        XCTAssertEqual(repo.estado(escenaId: "B").cuadernoId, "dos")
        XCTAssertNil(repo.estado(escenaId: "B").pestanaSeleccionadaId)
    }

    func testCerrarDocumentoConservaIdentidadDeEscenaYAislaLaOtra() throws {
        let repo = RepositorioEstadoEscenasCuadernUIPad(base: try base())
        var a = EstadoEscenaCuadernUIPad(escenaId: "A")
        a.cuadernoId = "uno"
        a.pestanaSeleccionadaId = UUID()
        a.referenciaVisible = true
        var b = EstadoEscenaCuadernUIPad(escenaId: "B")
        b.cuadernoId = "dos"
        XCTAssertTrue(repo.guardar(a)); XCTAssertTrue(repo.guardar(b))

        XCTAssertTrue(repo.actualizar(escenaId: "A") { $0.cerrarDocumento() })
        let cerrada = repo.estado(escenaId: "A")
        XCTAssertEqual(cerrada.escenaId, "A")
        XCTAssertNil(cerrada.cuadernoId)
        XCTAssertNil(cerrada.documentoId)
        XCTAssertNil(cerrada.pestanaSeleccionadaId)
        XCTAssertNil(cerrada.referenciaVisible)
        XCTAssertEqual(cerrada.panelLateral, .ninguno)
        XCTAssertEqual(repo.estado(escenaId: "B").cuadernoId, "dos")
    }

    func testSesionPorIdentificadorRechazaCambiarDeEscena() throws {
        let repo = RepositorioEstadoEscenasCuadernUIPad(base: try base())
        let sesion = SesionEscenaCuadernUIPad(escenaId: "A", repo: repo)
        XCTAssertTrue(sesion.actualizar { $0.cuadernoId = "uno" })
        XCTAssertEqual(sesion.estado.cuadernoId, "uno")

        var ajena = EstadoEscenaCuadernUIPad(escenaId: "B")
        ajena.cuadernoId = "dos"
        XCTAssertFalse(sesion.guardar(ajena))
        XCTAssertEqual(repo.estado(escenaId: "A").cuadernoId, "uno")
        XCTAssertNil(repo.estado(escenaId: "B").cuadernoId)
    }

    func testRegistrarAperturaUsaCuadernoExactoYDescartaSnapshotAnterior() throws {
        let raiz = try base()
        let repoEscenas = RepositorioEstadoEscenasCuadernUIPad(
            base: raiz.appendingPathComponent("estado", isDirectory: true))
        let repoPestanas = RepositorioPestanasIPad(
            directorioBase: raiz.appendingPathComponent("pestanas", isDirectory: true))
        let coordinador = CoordinadorEscenasCuadernUIPad(
            repoEscenas: repoEscenas, repoPestanas: repoPestanas)

        let vieja = UUID()
        var escenaA = EstadoEscenaCuadernUIPad(escenaId: "A")
        escenaA.cuadernoId = "anterior"
        escenaA.pestanaSeleccionadaId = vieja
        escenaA.documentoId = DocumentoPersistenteID(vieja)
        escenaA.referenciaVisible = true
        var escenaB = EstadoEscenaCuadernUIPad(escenaId: "B")
        escenaB.cuadernoId = "independiente"
        XCTAssertTrue(repoEscenas.guardar(escenaA))
        XCTAssertTrue(repoEscenas.guardar(escenaB))

        XCTAssertTrue(coordinador.registrarApertura(
            cuadernoId: "nuevo-exacto", escenaId: "A"))
        let nuevaA = repoEscenas.estado(escenaId: "A")
        XCTAssertEqual(nuevaA.cuadernoId, "nuevo-exacto")
        XCTAssertNil(nuevaA.pestanaSeleccionadaId)
        XCTAssertNil(nuevaA.documentoId)
        XCTAssertNil(nuevaA.referenciaVisible)
        XCTAssertEqual(repoEscenas.estado(escenaId: "B").cuadernoId,
                       "independiente")
    }
}
