import UIKit
import XCTest
@testable import CuadernUEstudio

final class BibliotecaCuadernosTests: XCTestCase {
    func testPrimeraEjecucionConservaElContextoAnteriorUnaSolaVez() throws {
        let (repositorio, directorio) = try crearRepositorioTemporal()
        defer { try? FileManager.default.removeItem(at: directorio) }

        let creado = try XCTUnwrap(repositorio.prepararPrimeraEjecucion(
            contextoAnterior: "ESCENA-ANTERIOR-123"))
        XCTAssertEqual(creado.id, "ESCENA-ANTERIOR-123")
        XCTAssertEqual(creado.titulo, "Mi cuaderno")
        let guardados = try repositorio.listar()
        XCTAssertEqual(guardados.count, 1)
        let guardado = try XCTUnwrap(guardados.first)
        XCTAssertEqual(guardado.id, creado.id)
        XCTAssertEqual(guardado.titulo, creado.titulo)
        XCTAssertEqual(guardado.indiceColor, creado.indiceColor)
        XCTAssertEqual(guardado.fechaCreacion.timeIntervalSince1970,
                       creado.fechaCreacion.timeIntervalSince1970,
                       accuracy: 0.001)
        XCTAssertEqual(guardado.fechaModificacion.timeIntervalSince1970,
                       creado.fechaModificacion.timeIntervalSince1970,
                       accuracy: 0.001)

        XCTAssertNil(try repositorio.prepararPrimeraEjecucion(
            contextoAnterior: "OTRA-ESCENA"))
        _ = try repositorio.eliminar(id: creado.id)
        XCTAssertNil(try repositorio.prepararPrimeraEjecucion(
            contextoAnterior: "NO-DEBE-RECREARSE"))
        XCTAssertTrue(try repositorio.listar().isEmpty)
    }

    func testCrearRenombrarColorYReabrirCatalogo() throws {
        let (repositorio, directorio) = try crearRepositorioTemporal()
        defer { try? FileManager.default.removeItem(at: directorio) }
        let fechaInicial = Date(timeIntervalSince1970: 1_700_000_000)
        let fechaCambio = fechaInicial.addingTimeInterval(60)

        let creado = try repositorio.crear(
            titulo: "  Circuitos I  ", identificador: "CUADERNO-1",
            fecha: fechaInicial)
        XCTAssertEqual(creado.titulo, "Circuitos I")
        XCTAssertTrue((0..<RepositorioBibliotecaCuadernU.cantidadColores)
            .contains(creado.indiceColor))

        _ = try repositorio.renombrar(
            id: creado.id, titulo: "Circuitos II", fecha: fechaCambio)
        let cambiado = try repositorio.cambiarColor(
            id: creado.id, indiceColor: -1, fecha: fechaCambio)
        XCTAssertEqual(cambiado.indiceColor, 7)

        let reabierto = RepositorioBibliotecaCuadernU(
            directorioBase: directorio)
        let guardado = try XCTUnwrap(reabierto.listar().first)
        XCTAssertEqual(guardado.id, creado.id)
        XCTAssertEqual(guardado.titulo, "Circuitos II")
        XCTAssertEqual(guardado.fechaModificacion, fechaCambio)
        XCTAssertEqual(guardado.indiceColor, 7)
    }

    func testMigracionIncorporaTodasLasEscenasAntiguasSinRecrearEliminadas() throws {
        let (repositorio, directorio) = try crearRepositorioTemporal()
        defer { try? FileManager.default.removeItem(at: directorio) }

        _ = try repositorio.prepararPrimeraEjecucion(
            contextoAnterior: "ESCENA-A",
            contextosPersistidos: ["ESCENA-A", "ESCENA-B", "ESCENA-C"])
        XCTAssertEqual(Set(try repositorio.listar().map(\.id)),
                       ["ESCENA-A", "ESCENA-B", "ESCENA-C"])

        _ = try repositorio.eliminar(id: "ESCENA-A")
        XCTAssertNil(try repositorio.prepararPrimeraEjecucion(
            contextoAnterior: "ESCENA-NUEVA-SIN-DATOS"))
        XCTAssertFalse(try repositorio.listar().contains {
            $0.id == "ESCENA-NUEVA-SIN-DATOS" || $0.id == "ESCENA-A"
        })

        let recuperado = try repositorio.prepararPrimeraEjecucion(
            contextoAnterior: "IGNORADA",
            contextosPersistidos: ["ESCENA-D-CON-DATOS"])
        XCTAssertNil(recuperado,
                     "La migración no debe reactivar contextos descubiertos después del marcador.")
        XCTAssertFalse(try repositorio.listar().contains {
            $0.id == "ESCENA-D-CON-DATOS"
        })

        // Incluso una restauración parcial que pierda el marcador global no
        // puede resucitar un ID eliminado expresamente.
        let marcador = directorio.appendingPathComponent(
            ".biblioteca-inicializada-v1")
        try FileManager.default.removeItem(at: marcador)
        _ = try repositorio.prepararPrimeraEjecucion(
            contextoAnterior: "ESCENA-A",
            contextosPersistidos: ["ESCENA-A"])
        XCTAssertFalse(try repositorio.listar().contains { $0.id == "ESCENA-A" })
    }

    func testMigracionNoFusionaDosContextosQueNormalizanAlMismoNombre() throws {
        let (repositorio, directorio) = try crearRepositorioTemporal()
        defer { try? FileManager.default.removeItem(at: directorio) }

        XCTAssertThrowsError(try repositorio.prepararPrimeraEjecucion(
            contextoAnterior: "escena/a",
            contextosPersistidos: ["escena/a", "escena_a"])) {
            XCTAssertEqual($0 as? ErrorBibliotecaCuadernU,
                           .identificadorInvalido)
        }
        XCTAssertTrue(try repositorio.listar().isEmpty)
    }

    func testCatalogoDanadoNoSeInterpretaComoVacioNiSeSobrescribe() throws {
        let (repositorio, directorio) = try crearRepositorioTemporal()
        defer { try? FileManager.default.removeItem(at: directorio) }
        let catalogo = directorio.appendingPathComponent("cuadernos-v1.json")
        let original = Data("{ catálogo incompleto".utf8)
        try original.write(to: catalogo, options: .atomic)

        XCTAssertThrowsError(try repositorio.listar()) {
            XCTAssertEqual($0 as? ErrorBibliotecaCuadernU, .catalogoDanado)
        }
        XCTAssertThrowsError(try repositorio.crear(titulo: "No sobrescribir")) {
            XCTAssertEqual($0 as? ErrorBibliotecaCuadernU, .catalogoDanado)
        }
        XCTAssertEqual(try Data(contentsOf: catalogo), original)
    }

    func testTitulosVaciosIdentificadoresYColoresSeValidan() throws {
        let (repositorio, directorio) = try crearRepositorioTemporal()
        defer { try? FileManager.default.removeItem(at: directorio) }

        XCTAssertThrowsError(try repositorio.crear(titulo: "  \n  ")) {
            XCTAssertEqual($0 as? ErrorBibliotecaCuadernU, .tituloVacio)
        }
        let convertido = try repositorio.crear(
            titulo: "Álgebra", identificador: "escena/con espacios")
        XCTAssertEqual(convertido.id, "escena_con_espacios")
        XCTAssertEqual(RepositorioBibliotecaCuadernU.normalizarColor(-9), 7)
        XCTAssertEqual(RepositorioBibliotecaCuadernU.normalizarColor(17), 1)
        XCTAssertEqual(
            RepositorioBibliotecaCuadernU.colorDeterminista(para: convertido.id),
            RepositorioBibliotecaCuadernU.colorDeterminista(para: convertido.id))
        XCTAssertEqual(CeldaCuadernoIPad.nombresColoresPortada.count,
                       CeldaCuadernoIPad.coloresPortada.count)
        XCTAssertEqual(CeldaCuadernoIPad.nombresColoresPortada,
                       ["Azul", "Rojo", "Verde", "Amarillo",
                        "Morado", "Naranja", "Turquesa", "Rosa"])
    }

    func testIdentidadDocumentalDurableNoDependeDelHandleDelNucleo() throws {
        let uuid = try XCTUnwrap(UUID(
            uuidString: "7A9DD9A8-1F11-4B1C-A13A-D80518A7B338"))
        let identidad = DocumentoPersistenteID(uuid)
        let codificada = try JSONEncoder().encode(identidad)
        let reabierta = try JSONDecoder().decode(
            DocumentoPersistenteID.self, from: codificada)

        XCTAssertEqual(reabierta, identidad)
        XCTAssertEqual(reabierta.description,
                       "7a9dd9a8-1f11-4b1c-a13a-d80518a7b338")
        XCTAssertEqual(identidad.claveCompatibilidadUInt64,
                       12_114_170_925_309_742_044,
                       "Cambiar este vector dejaría inaccesibles sidecars v2.")
        XCTAssertNotEqual(identidad.claveCompatibilidadUInt64, 0)
        XCTAssertNotEqual(
            identidad.claveCompatibilidadUInt64 & (UInt64(1) << 63), 0,
                          "La clave v2 no debe colisionar con handles legacy pequeños.")

        let primera = try XCTUnwrap(Sesion(idPersistente: identidad))
        let segunda = try XCTUnwrap(Sesion(idPersistente: identidad))
        XCTAssertNotEqual(primera.manejadorNucleo, segunda.manejadorNucleo)
        XCTAssertEqual(primera.idPersistente, segunda.idPersistente)
        XCTAssertEqual(primera.documentoPersistenteId,
                       segunda.documentoPersistenteId)
    }

    func testUnCuadernoSoloAdmiteUnEscritorEntreEscenas() throws {
        let id = "LEASE-\(UUID().uuidString)"
        do {
            let primero = try XCTUnwrap(
                CentroAperturasCuadernosIPad.compartido.adquirir(id))
            XCTAssertNil(CentroAperturasCuadernosIPad.compartido.adquirir(id))
            withExtendedLifetime(primero) {}
        }
        XCTAssertNotNil(CentroAperturasCuadernosIPad.compartido.adquirir(id))
    }

    @MainActor
    func testBibliotecaVaciaSiempreMuestraAccionDeCreacion() throws {
        let gestor = GestorBibliotecaFalso(registros: [])
        let controlador = BibliotecaCuadernosIPad(gestor: gestor)
        let navegacion = UINavigationController(rootViewController: controlador)
        navegacion.loadViewIfNeeded()
        controlador.loadViewIfNeeded()
        controlador.view.frame = CGRect(x: 0, y: 0, width: 1024, height: 768)
        controlador.view.layoutIfNeeded()

        XCTAssertNotNil(controlador.navigationItem.rightBarButtonItem)
        let boton = try XCTUnwrap(controlador.view.descendiente(
            identificador: "biblioteca.crear.primero") as? UIButton)
        XCTAssertFalse(boton.isHidden)
        XCTAssertTrue(boton.isDescendant(of: controlador.view))
        let estado = try XCTUnwrap(controlador.view.descendiente(
            identificador: "biblioteca.estado.vacio"))
        XCTAssertFalse(estado.isHidden)
    }

    @MainActor
    func testBibliotecaConCuadernosOcultaElEstadoVacioYMuestraLaGrilla() throws {
        let fecha = Date(timeIntervalSince1970: 1_700_000_000)
        let registro = CuadernoBibliotecaIPad(
            id: "CUADERNO-PRUEBA", titulo: "Electrónica",
            fechaCreacion: fecha, fechaModificacion: fecha, indiceColor: 2)
        let controlador = BibliotecaCuadernosIPad(
            gestor: GestorBibliotecaFalso(registros: [registro]))
        controlador.loadViewIfNeeded()

        let coleccion = try XCTUnwrap(controlador.view.descendiente(
            identificador: "biblioteca.cuadernos") as? UICollectionView)
        XCTAssertEqual(controlador.collectionView(
            coleccion, numberOfItemsInSection: 0), 1)
        let estado = try XCTUnwrap(controlador.view.descendiente(
            identificador: "biblioteca.estado.vacio"))
        XCTAssertTrue(estado.isHidden)
    }

    @MainActor
    func testEditorDelCuadernoExponePanelPrincipalYAccionDeNuevaPestana() throws {
        let controlador = ControladorRaizIPad(
            contextoEscena: "CUADERNO-EDITOR-PRUEBA")
        controlador.loadViewIfNeeded()
        controlador.view.frame = CGRect(x: 0, y: 0, width: 1024, height: 768)
        controlador.view.layoutIfNeeded()

        XCTAssertEqual(controlador.view.accessibilityIdentifier, "editor.raiz")
        let panel = try XCTUnwrap(controlador.viewController(for: .secondary))
        panel.loadViewIfNeeded()
        XCTAssertEqual(panel.view.accessibilityIdentifier,
                       "editor.panel.principal")
        let nuevaPestana = try XCTUnwrap(panel.view.descendiente(
            identificador: "editor.pestana.nueva") as? UIButton)
        XCTAssertFalse(nuevaPestana.isHidden)
        XCTAssertTrue(nuevaPestana.isUserInteractionEnabled)
        XCTAssertNotNil(nuevaPestana.menu)

        let referencia = try XCTUnwrap(
            controlador.viewController(for: .primary)
                as? ControladorPestanasDocumentoIPad)
        if !referencia.isViewLoaded {
            try referencia.guardarAhora()
            XCTAssertFalse(referencia.isViewLoaded,
                           "Guardar no debe cargar ni vaciar un panel diferido.")
        }
    }

    @MainActor
    func testGuardarPanelDiferidoConservaManifestoSecundario() throws {
        let directorio = FileManager.default.temporaryDirectory
            .appendingPathComponent(
                "CuadernU-Panel-Diferido-\(UUID().uuidString)",
                isDirectory: true)
        try FileManager.default.createDirectory(
            at: directorio, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: directorio) }

        let repositorio = RepositorioPestanasIPad(directorioBase: directorio)
        let primeraId = UUID()
        let segundaId = UUID()
        let primera = RegistroPestanaIPad(
            id: primeraId,
            titulo: "Referencia uno", colorARGB: 0xFF0A84FF,
            archivo: "secundario-\(primeraId.uuidString).tinta")
        let segunda = RegistroPestanaIPad(
            id: segundaId,
            titulo: "Referencia dos", colorARGB: 0xFF30D158,
            archivo: "secundario-\(segundaId.uuidString).tinta")
        var esperado = EstadoPestanasIPad()
        esperado.abiertas = [primera, segunda]
        esperado.activa = segunda.id
        try repositorio.guardar(
            esperado, panel: .secundario, contexto: "CUADERNO-DIFERIDO")

        let controlador = ControladorPestanasDocumentoIPad(
            panel: .secundario, viewport: .secundario,
            contextoEscena: "CUADERNO-DIFERIDO", repositorio: repositorio)
        XCTAssertFalse(controlador.isViewLoaded)
        try controlador.guardarAhora()
        XCTAssertFalse(controlador.isViewLoaded)

        guard case .cargado(let conservado) = repositorio.leer(
            .secundario, contexto: "CUADERNO-DIFERIDO") else {
            return XCTFail("El manifiesto secundario dejó de ser legible.")
        }
        XCTAssertEqual(conservado, esperado)
    }

    @MainActor
    func testManifestoDanadoSeConservaYNoSeReemplazaPorPestanaVacia() throws {
        let directorio = FileManager.default.temporaryDirectory
            .appendingPathComponent(
                "CuadernU-Panel-Danado-\(UUID().uuidString)",
                isDirectory: true)
        try FileManager.default.createDirectory(
            at: directorio, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: directorio) }
        let ruta = directorio.appendingPathComponent(
            "secundario-CUADERNO-DANADO.json")
        let original = Data("{ manifiesto truncado".utf8)
        try original.write(to: ruta, options: .atomic)

        let repositorio = RepositorioPestanasIPad(directorioBase: directorio)
        let controlador = ControladorPestanasDocumentoIPad(
            panel: .secundario, viewport: .secundario,
            contextoEscena: "CUADERNO-DANADO", repositorio: repositorio)
        controlador.loadViewIfNeeded()
        try controlador.guardarAhora()

        XCTAssertNotNil(controlador.view.descendiente(
            identificador: "editor.manifiesto.danado"))
        XCTAssertEqual(try Data(contentsOf: ruta), original)
        guard case .danado = repositorio.leer(
            .secundario, contexto: "CUADERNO-DANADO") else {
            return XCTFail("Un manifiesto corrupto no debe parecer inexistente.")
        }
    }

    @MainActor
    func testContenidoDanadoNoSeSobrescribeConLienzoVacio() throws {
        let directorio = try crearDirectorioPestanasTemporal(
            prefijo: "CuadernU-Contenido-Danado")
        defer { try? FileManager.default.removeItem(at: directorio) }
        let repositorio = RepositorioPestanasIPad(directorioBase: directorio)
        let registro = RegistroPestanaIPad(
            id: UUID(uuidString: "11111111-2222-3333-4444-555555555555")!,
            titulo: "Original", colorARGB: 0xFF0A84FF,
            archivo: "principal-11111111-2222-3333-4444-555555555555.tinta")
        var estado = EstadoPestanasIPad()
        estado.abiertas = [registro]
        estado.activa = registro.id
        try repositorio.guardar(estado, panel: .principal,
                                contexto: "CONTENIDO-DANADO")
        let original = Data("bytes truncados".utf8)
        try repositorio.guardarContenido(original, registro: registro)

        let controlador = ControladorPestanasDocumentoIPad(
            panel: .principal, viewport: .principal,
            contextoEscena: "CONTENIDO-DANADO", repositorio: repositorio)
        controlador.loadViewIfNeeded()
        try controlador.guardarAhora()

        guard case .cargado(let conservado) = repositorio.contenido(registro)
        else { return XCTFail("El archivo original dejó de ser legible.") }
        XCTAssertEqual(conservado, original)
    }

    @MainActor
    func testPrimerEditorConfirmaManifestoYContenidoAntesDeUsarse() throws {
        let directorio = try crearDirectorioPestanasTemporal(
            prefijo: "CuadernU-Primer-Checkpoint")
        defer { try? FileManager.default.removeItem(at: directorio) }
        let repositorio = RepositorioPestanasIPad(directorioBase: directorio)
        let controlador = ControladorPestanasDocumentoIPad(
            panel: .principal, viewport: .principal,
            contextoEscena: "PRIMER-CHECKPOINT", repositorio: repositorio)
        controlador.loadViewIfNeeded()

        guard case .cargado(let estado) = repositorio.leer(
            .principal, contexto: "PRIMER-CHECKPOINT"),
              let registro = estado.abiertas.first else {
            return XCTFail("El editor quedó utilizable sin manifiesto durable.")
        }
        guard case .cargado(let contenido) = repositorio.contenido(registro)
        else { return XCTFail("El manifiesto referencia contenido inexistente.") }
        XCTAssertFalse(contenido.isEmpty)
    }

    @MainActor
    func testPestanasInactivasEntreganSusRecursosMetal() throws {
        let directorio = try crearDirectorioPestanasTemporal(
            prefijo: "CuadernU-Presupuesto-Metal")
        defer { try? FileManager.default.removeItem(at: directorio) }
        let repositorio = RepositorioPestanasIPad(directorioBase: directorio)

        let registros = (0..<3).map { indice in
            let id = UUID()
            return RegistroPestanaIPad(
                id: id, titulo: "Pestaña \(indice + 1)",
                colorARGB: 0xFF0A84FF,
                archivo: "principal-\(id.uuidString).tinta")
        }
        var estado = EstadoPestanasIPad()
        estado.abiertas = registros
        estado.activa = registros[1].id
        try repositorio.guardar(
            estado, panel: .principal, contexto: "PRESUPUESTO-METAL")
        for registro in registros {
            let sesion = try XCTUnwrap(Sesion(
                idPersistente: DocumentoPersistenteID(registro.id)))
            let contenido = try XCTUnwrap(sesion.guardar())
            try repositorio.guardarContenido(contenido, registro: registro)
        }

        let controlador = ControladorPestanasDocumentoIPad(
            panel: .principal, viewport: .principal,
            contextoEscena: "PRESUPUESTO-METAL", repositorio: repositorio)
        controlador.loadViewIfNeeded()
        controlador.view.frame = CGRect(x: 0, y: 0, width: 1024, height: 768)
        controlador.view.layoutIfNeeded()

        XCTAssertEqual(controlador.cantidadLienzosConRecursosGraficos, 1)
        XCTAssertTrue(controlador.activarPestana(id: registros[2].id))
        XCTAssertEqual(controlador.cantidadLienzosConRecursosGraficos, 1)
        XCTAssertFalse(controlador.activarPestana(id: UUID()))
        XCTAssertEqual(controlador.cantidadLienzosConRecursosGraficos, 1)

        controlador.viewDidDisappear(false)
        XCTAssertEqual(controlador.cantidadLienzosConRecursosGraficos, 0)
    }

    private func crearRepositorioTemporal() throws
        -> (RepositorioBibliotecaCuadernU, URL) {
        let directorio = FileManager.default.temporaryDirectory
            .appendingPathComponent("CuadernU-Biblioteca-Tests-\(UUID().uuidString)",
                                    isDirectory: true)
        try FileManager.default.createDirectory(
            at: directorio, withIntermediateDirectories: true)
        return (RepositorioBibliotecaCuadernU(directorioBase: directorio),
                directorio)
    }

    private func crearDirectorioPestanasTemporal(prefijo: String) throws
        -> URL {
        let directorio = FileManager.default.temporaryDirectory
            .appendingPathComponent("\(prefijo)-\(UUID().uuidString)",
                                    isDirectory: true)
        try FileManager.default.createDirectory(
            at: directorio, withIntermediateDirectories: true)
        return directorio
    }
}

final class EliminacionPestanasTests: XCTestCase {
    func testReferenciaCompartidaEntreManifiestosBloqueaElBorrado() throws {
        let directorio = try crearDirectorioTemporal(
            prefijo: "CuadernU-Referencia-Cruzada")
        defer { try? FileManager.default.removeItem(at: directorio) }
        let repositorio = RepositorioPestanasIPad(directorioBase: directorio)
        let id = UUID()
        let registro = RegistroPestanaIPad(
            id: id, titulo: "Compartido", colorARGB: 0xFF0A84FF,
            archivo: "principal-\(id.uuidString).tinta")
        var estado = EstadoPestanasIPad()
        estado.abiertas = [registro]
        estado.activa = id
        try repositorio.guardar(estado, panel: .principal, contexto: "A")
        try repositorio.guardar(estado, panel: .principal, contexto: "B")
        try repositorio.guardarContenido(Data("original".utf8),
                                         registro: registro)

        XCTAssertThrowsError(
            try repositorio.prepararEliminacion(contexto: "A"))
        XCTAssertTrue(FileManager.default.fileExists(atPath:
            directorio.appendingPathComponent(registro.archivo).path))
    }

    func testTerminacionAntesDeQuitarLaTarjetaRestauraContenido() throws {
        let (repositorio, directorio, registro) = try prepararCuadernoTemporal()
        defer { try? FileManager.default.removeItem(at: directorio) }

        _ = try repositorio.prepararEliminacion(contexto: "CUADERNO-A")
        XCTAssertFalse(FileManager.default.fileExists(atPath:
            directorio.appendingPathComponent(registro.archivo).path))

        let reiniciado = RepositorioPestanasIPad(directorioBase: directorio)
        try reiniciado.recuperarEliminacionesInterrumpidas(
            contextosExistentes: ["CUADERNO-A"])

        XCTAssertTrue(FileManager.default.fileExists(atPath:
            directorio.appendingPathComponent(registro.archivo).path))
        XCTAssertTrue(FileManager.default.fileExists(atPath:
            directorio.appendingPathComponent("principal-CUADERNO-A.json").path))
        XCTAssertEqual(try reiniciado.contextosPersistidos(), ["CUADERNO-A"])
        XCTAssertTrue(try carpetasTransitorias(en: directorio).isEmpty)
    }

    func testTerminacionDespuesDeQuitarLaTarjetaCompletaElBorrado() throws {
        let (repositorio, directorio, registro) = try prepararCuadernoTemporal()
        defer { try? FileManager.default.removeItem(at: directorio) }

        _ = try repositorio.prepararEliminacion(contexto: "CUADERNO-A")
        let reiniciado = RepositorioPestanasIPad(directorioBase: directorio)
        try reiniciado.recuperarEliminacionesInterrumpidas(
            contextosExistentes: [])

        XCTAssertFalse(FileManager.default.fileExists(atPath:
            directorio.appendingPathComponent(registro.archivo).path))
        XCTAssertFalse(FileManager.default.fileExists(atPath:
            directorio.appendingPathComponent("principal-CUADERNO-A.json").path))
        XCTAssertTrue(try carpetasTransitorias(en: directorio).isEmpty)
    }

    func testDiarioIlegibleSeConservaSinTocar() throws {
        let directorio = try crearDirectorioTemporal(
            prefijo: "CuadernU-Diario-Danado")
        defer { try? FileManager.default.removeItem(at: directorio) }
        let carpeta = directorio.appendingPathComponent(
            ".eliminacion-DANADA", isDirectory: true)
        try FileManager.default.createDirectory(
            at: carpeta, withIntermediateDirectories: true)
        let diario = carpeta.appendingPathComponent("transaccion.json")
        let original = Data("{incompleto".utf8)
        try original.write(to: diario)

        XCTAssertThrowsError(try RepositorioPestanasIPad(
            directorioBase: directorio)
            .recuperarEliminacionesInterrumpidas(contextosExistentes: []))

        XCTAssertTrue(FileManager.default.fileExists(atPath: carpeta.path))
        XCTAssertEqual(try Data(contentsOf: diario), original)
    }

    private func prepararCuadernoTemporal() throws
        -> (RepositorioPestanasIPad, URL, RegistroPestanaIPad) {
        let directorio = try crearDirectorioTemporal(
            prefijo: "CuadernU-Eliminacion-Tests")
        let repositorio = RepositorioPestanasIPad(directorioBase: directorio)
        let registroId = UUID()
        let registro = RegistroPestanaIPad(
            id: registroId,
            titulo: "Apuntes", colorARGB: 0xFF1A_1A1A,
            archivo: "principal-\(registroId.uuidString).tinta")
        var estado = EstadoPestanasIPad()
        estado.abiertas = [registro]
        estado.activa = registro.id
        try repositorio.guardar(estado, panel: .principal,
                                contexto: "CUADERNO-A")
        try repositorio.guardarContenido(Data("contenido".utf8),
                                         registro: registro)
        return (repositorio, directorio, registro)
    }

    private func crearDirectorioTemporal(prefijo: String) throws -> URL {
        let directorio = FileManager.default.temporaryDirectory
            .appendingPathComponent("\(prefijo)-\(UUID().uuidString)",
                                    isDirectory: true)
        try FileManager.default.createDirectory(
            at: directorio, withIntermediateDirectories: true)
        return directorio
    }

    private func carpetasTransitorias(en directorio: URL) throws -> [URL] {
        try FileManager.default.contentsOfDirectory(
            at: directorio, includingPropertiesForKeys: nil)
            .filter { $0.lastPathComponent.hasPrefix(".eliminacion-") }
    }
}

private final class GestorBibliotecaFalso: GestionBibliotecaCuadernU {
    var registros: [CuadernoBibliotecaIPad]

    init(registros: [CuadernoBibliotecaIPad]) {
        self.registros = registros
    }

    func listarCuadernos() throws -> [CuadernoBibliotecaIPad] { registros }

    func crearCuaderno(titulo: String) throws -> CuadernoBibliotecaIPad {
        let fecha = Date()
        let registro = CuadernoBibliotecaIPad(
            id: UUID().uuidString,
            titulo: try RepositorioBibliotecaCuadernU.normalizarTitulo(titulo),
            fechaCreacion: fecha, fechaModificacion: fecha, indiceColor: 0)
        registros.append(registro)
        return registro
    }

    func renombrarCuaderno(id: String, nombre: String) throws {
        guard let indice = registros.firstIndex(where: { $0.id == id }) else {
            throw ErrorBibliotecaCuadernU.cuadernoNoEncontrado
        }
        registros[indice].titulo = try RepositorioBibliotecaCuadernU
            .normalizarTitulo(nombre)
    }

    func actualizarColor(id: String, indiceColor: Int) throws {
        guard let indice = registros.firstIndex(where: { $0.id == id }) else {
            throw ErrorBibliotecaCuadernU.cuadernoNoEncontrado
        }
        registros[indice].indiceColor = RepositorioBibliotecaCuadernU
            .normalizarColor(indiceColor)
    }

    func eliminarCuaderno(id: String) throws {
        guard let indice = registros.firstIndex(where: { $0.id == id }) else {
            throw ErrorBibliotecaCuadernU.cuadernoNoEncontrado
        }
        registros.remove(at: indice)
    }
}

private extension UIView {
    func descendiente(identificador: String) -> UIView? {
        if accessibilityIdentifier == identificador { return self }
        for subvista in subviews {
            if let encontrada = subvista.descendiente(
                identificador: identificador) {
                return encontrada
            }
        }
        return nil
    }
}
