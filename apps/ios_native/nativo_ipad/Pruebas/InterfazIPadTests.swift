import UIKit
import XCTest
@testable import CuadernUEstudio

final class InterfazIPadTests: XCTestCase {
    func testRaizArrancaConUnSoloCuadernoPrincipal() {
        let principal = UIViewController()
        let referencia = UIViewController()
        let raiz = ControladorRaizIPad(principal: principal, referencia: referencia)

        XCTAssertEqual(raiz.preferredDisplayMode, .secondaryOnly)
        XCTAssertTrue(raiz.viewController(for: .secondary) === principal)
        XCTAssertTrue(raiz.viewController(for: .primary) === referencia)
        XCTAssertFalse(raiz.referenciaVisible)

        raiz.alternarReferencia()
        XCTAssertTrue(raiz.referenciaVisible)
        XCTAssertEqual(raiz.preferredDisplayMode, .oneBesideSecondary)

        raiz.alternarReferencia()
        XCTAssertFalse(raiz.referenciaVisible)
        XCTAssertEqual(raiz.preferredDisplayMode, .secondaryOnly)
    }

    func testCapaDeNotasNoBloqueaElFondoDelLienzo() {
        let capa = CapaNotasAdhesivasIPad(frame: CGRect(x: 0, y: 0,
                                                       width: 500, height: 500))
        XCTAssertNil(capa.hitTest(CGPoint(x: 250, y: 250), with: nil))
    }

    func testCapaDeNotasConservaInteraccionDeSusObjetos() {
        let capa = CapaNotasAdhesivasIPad(frame: CGRect(x: 0, y: 0,
                                                       width: 500, height: 500))
        let nota = UIControl(frame: CGRect(x: 40, y: 50, width: 120, height: 80))
        capa.addSubview(nota)

        XCTAssertTrue(capa.hitTest(CGPoint(x: 60, y: 70), with: nil) === nota)
        XCTAssertNil(capa.hitTest(CGPoint(x: 300, y: 300), with: nil))
    }

    func testInterfazNuevaEsSimpleYSinTelemetria() {
        let configuracion = ConfiguracionInterfazIPad()
        XCTAssertEqual(configuracion.nivel, .simple)
        XCTAssertFalse(configuracion.mostrarIndicadores)
        XCTAssertTrue(configuracion.barraCompacta)
        XCTAssertTrue(configuracion.ocultarSugerencias)
    }

    func testHostUnitarioNoArrancaLaEscenaCompletaNiConfundeUITest() {
        XCTAssertTrue(EntornoEjecucionIPad.esHostPruebasUnitarias(
            argumentos: ["CuadernUEstudio"],
            entorno: ["XCTestConfigurationFilePath": "/tmp/pruebas.xctestconfiguration"]))
        XCTAssertTrue(EntornoEjecucionIPad.esHostPruebasUnitarias(
            argumentos: ["CuadernUEstudio"],
            entorno: ["XCTestBundlePath": "/tmp/CuadernUEstudioTests.xctest"]))
        XCTAssertFalse(EntornoEjecucionIPad.esHostPruebasUnitarias(
            argumentos: ["CuadernUEstudio", "--pruebas-ui"],
            entorno: ["XCTestConfigurationFilePath": "/tmp/ui.xctestconfiguration"]))
        XCTAssertFalse(EntornoEjecucionIPad.esHostPruebasUnitarias(
            argumentos: ["CuadernUEstudio"], entorno: [:]))
    }

    func testAutoguardadoNoLlenaLaBarraConUnaHora() {
        let fecha = Date(timeIntervalSince1970: 1_700_000_000)
        let estado = EstadoAutoguardado.guardado(fecha)

        XCTAssertEqual(estado.texto, "Guardado")
        XCTAssertNotNil(estado.detalleAccesible)
        XCTAssertFalse(estado.texto.contains(":"))
        XCTAssertEqual(EstadoAutoguardado.fallo("Disco lleno").texto,
                       "No guardado")
    }

    func testJournalRechazaTamanoHostilAntesDeReservarMemoria() {
        let minimo = Data([0x01])
        XCTAssertNil(CompresorDatosLocal.descomprimir(
            minimo,
            tamanoOriginal: LimitesAutoguardadoTinta.cargaClaraMaxima + 1,
            algoritmo: CompresionCuaderno.equilibrada.algoritmo))
        XCTAssertNil(CompresorDatosLocal.descomprimir(
            minimo, tamanoOriginal: 2, algoritmo: nil),
            "Sin compresión también debe validar el tamaño declarado.")
        XCTAssertEqual(CompresorDatosLocal.descomprimir(
            minimo, tamanoOriginal: 1, algoritmo: nil), minimo)
    }

    func testGuardadoViejoNoConfirmaUnaEdicionMasReciente() {
        var generaciones = GeneracionesAutoguardado()
        let primera = generaciones.marcarCambio()
        let segunda = generaciones.marcarCambio()

        XCTAssertFalse(generaciones.confirmar(primera))
        XCTAssertTrue(generaciones.pendiente)
        XCTAssertTrue(generaciones.confirmar(segunda))
        XCTAssertFalse(generaciones.pendiente)
    }

    func testRepositorioPestanasEscribeElCanonicoSinBloquearElMain() throws {
        let base = FileManager.default.temporaryDirectory.appendingPathComponent(
            "tinta-prueba-canonico-\(UUID().uuidString)", isDirectory: true)
        defer { try? FileManager.default.removeItem(at: base) }
        let repositorio = RepositorioPestanasIPad(directorioBase: base)
        let id = UUID()
        let registro = RegistroPestanaIPad(
            id: id, titulo: "Prueba", colorARGB: 0xFF000000,
            archivo: "principal-\(id.uuidString).tinta")
        let esperado = Data("contenido-canónico".utf8)
        let termino = expectation(description: "escritura canónica")

        repositorio.guardarContenido(esperado, registro: registro) { resultado in
            XCTAssertTrue(Thread.isMainThread)
            XCTAssertNoThrow(try resultado.get())
            if case .cargado(let actual) = repositorio.contenido(registro) {
                XCTAssertEqual(actual, esperado)
            } else {
                XCTFail("El archivo escrito no pudo volver a leerse.")
            }
            termino.fulfill()
        }

        wait(for: [termino], timeout: 2)
    }

    func testRepositorioPestanasRechazaNombreHostilEnEscrituraAsincrona() {
        let base = FileManager.default.temporaryDirectory.appendingPathComponent(
            "tinta-prueba-ruta-\(UUID().uuidString)", isDirectory: true)
        defer { try? FileManager.default.removeItem(at: base) }
        let repositorio = RepositorioPestanasIPad(directorioBase: base)
        let registro = RegistroPestanaIPad(
            titulo: "Hostil", colorARGB: 0xFF000000,
            archivo: "../fuera.tinta")
        let termino = expectation(description: "ruta rechazada")

        repositorio.guardarContenido(Data([0x01]), registro: registro) { resultado in
            XCTAssertThrowsError(try resultado.get()) { error in
                XCTAssertEqual(error as? ErrorPersistenciaPestanasIPad,
                               .nombreArchivoInvalido)
            }
            termino.fulfill()
        }

        wait(for: [termino], timeout: 2)
    }

    func testCommitAutoguardadoConfirmaSoloJournalMasCanonico() throws {
        let esperado = Data("contenido".utf8)
        let journal = URL(fileURLWithPath: "/tmp/generacion.tintajournal")
        var orden: [String] = []
        var resultado: Result<URL, Error>?

        SecuenciaCommitAutoguardado.ejecutar(
            datosCanonicos: esperado,
            persistidorJournal: { terminar in
                orden.append("journal")
                terminar(.success(journal))
            },
            persistidorCanonico: { datos, terminar in
                orden.append("canonico")
                XCTAssertEqual(datos, esperado)
                terminar(.success(()))
            },
            terminar: { resultado = $0 })

        XCTAssertEqual(orden, ["journal", "canonico"])
        XCTAssertEqual(try resultado?.get(), journal)
    }

    func testCommitAutoguardadoNoOcultaFalloCanonico() {
        struct ErrorDisco: Error {}
        var resultado: Result<URL, Error>?

        SecuenciaCommitAutoguardado.ejecutar(
            datosCanonicos: Data([0x01]),
            persistidorJournal: { terminar in
                terminar(.success(URL(fileURLWithPath: "/tmp/journal")))
            },
            persistidorCanonico: { _, terminar in
                terminar(.failure(ErrorDisco()))
            },
            terminar: { resultado = $0 })

        XCTAssertThrowsError(try resultado?.get()) { error in
            XCTAssertTrue(error is ErrorDisco)
        }
    }

    func testRelojNotaVivaExcluyeDosPausasYNoReanudaDosVeces() {
        var reloj = RelojNotaViva()
        XCTAssertTrue(reloj.comenzar(en: 10))
        XCTAssertEqual(reloj.pausar(en: 14) ?? -1, 4, accuracy: 0.000_001)
        XCTAssertEqual(reloj.segundo(en: 90), 4, accuracy: 0.000_001)
        XCTAssertTrue(reloj.reanudar(en: 100))
        XCTAssertFalse(reloj.reanudar(en: 101))
        XCTAssertEqual(reloj.pausar(en: 105) ?? -1, 9, accuracy: 0.000_001)
        XCTAssertEqual(reloj.detener(en: 1_000), 9, accuracy: 0.000_001)
    }

    func testManifiestoNotaVivaV5SigueAbriendoTrasAgregarRecuperacionDeAudio() throws {
        let json = """
        {
          "version": 5,
          "id": "71275ED2-FE71-4CE8-B379-5000102F26EA",
          "creado": "2026-08-13T12:00:00Z",
          "inicioMonotono": 10,
          "duracion": 45,
          "archivoTinta": "nota.tinta",
          "bitrateAudioKbps": 128,
          "frecuenciaHz": 48000,
          "canales": 1,
          "segmentos": [],
          "eventos": [{"tipo":"inicio","segundo":0}]
        }
        """
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        let manifiesto = try decoder.decode(
            ManifiestoNotaViva.self, from: Data(json.utf8))
        XCTAssertEqual(manifiesto.version, 5)
        XCTAssertNil(manifiesto.archivoAudioParcial)
        XCTAssertNil(manifiesto.advertencias)
    }

    func testManifiestoNotaVivaNoPierdeActualizacionesConcurrentes() throws {
        let carpeta = FileManager.default.temporaryDirectory.appendingPathComponent(
            "prueba-manifiesto-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(
            at: carpeta, withIntermediateDirectories: false)
        addTeardownBlock { try? FileManager.default.removeItem(at: carpeta) }
        let url = carpeta.appendingPathComponent("manifest.json")
        let base = """
        {
          "version": 5,
          "id": "71275ED2-FE71-4CE8-B379-5000102F26EA",
          "creado": "2026-08-13T12:00:00Z",
          "inicioMonotono": 10,
          "duracion": 45,
          "archivoTinta": "nota.tinta",
          "bitrateAudioKbps": 128,
          "frecuenciaHz": 48000,
          "canales": 1,
          "segmentos": [],
          "eventos": [{"tipo":"inicio","segundo":0}]
        }
        """
        try Data(base.utf8).write(to: url, options: .atomic)

        let terminadas = expectation(description: "actualizaciones serializadas")
        terminadas.expectedFulfillmentCount = 40
        for indice in 0..<40 {
            DispatchQueue.global(qos: .userInitiated).async {
                _ = try? RepositorioManifiestoNotaViva.actualizar(url) {
                    $0.advertencias = ($0.advertencias ?? []) + ["aviso-\(indice)"]
                }
                terminadas.fulfill()
            }
        }
        wait(for: [terminadas], timeout: 10)

        let final = try RepositorioManifiestoNotaViva.cargar(url)
        XCTAssertEqual(Set(final.advertencias ?? []).count, 40)
    }

    func testQRWebSeConsumeYLaCookieMantieneSoloLaSesionCreada() {
        let inicio = Date(timeIntervalSince1970: 1_700_000_000)
        var estado = EstadoAutorizacionWebIPad(codigo: "codigo-inicial",
                                                duracion: 120)

        XCTAssertEqual(estado.abrir(
            codigo: "incorrecto", cookie: nil, nuevoToken: "sesion-a",
            ahora: inicio), .rechazada)
        XCTAssertEqual(estado.abrir(
            codigo: "codigo-inicial", cookie: nil, nuevoToken: "sesion-a",
            ahora: inicio),
            .creada(token: "sesion-a", siguienteSecuencia: 1))
        XCTAssertNil(estado.codigoActual,
                     "El QR no puede seguir siendo bearer reutilizable.")
        XCTAssertEqual(estado.abrir(
            codigo: "codigo-inicial", cookie: nil, nuevoToken: "sesion-b",
            ahora: inicio), .rechazada)
        XCTAssertEqual(estado.abrir(
            codigo: nil, cookie: "sesion-a", nuevoToken: "sesion-b",
            ahora: inicio), .existente(siguienteSecuencia: 1))
    }

    func testSesionWebRechazaReplayYSesionVencida() {
        let inicio = Date(timeIntervalSince1970: 1_700_000_000)
        var estado = EstadoAutorizacionWebIPad(codigo: "codigo", duracion: 60)
        XCTAssertEqual(estado.abrir(
            codigo: "codigo", cookie: nil, nuevoToken: "cookie", ahora: inicio),
                       .creada(token: "cookie", siguienteSecuencia: 1))

        XCTAssertEqual(estado.autorizarAPI(
            cookie: "cookie", secuencia: 1, ahora: inicio), .permitida)
        XCTAssertEqual(estado.autorizarAPI(
            cookie: "cookie", secuencia: 1, ahora: inicio), .secuenciaInvalida)
        XCTAssertEqual(estado.autorizarAPI(
            cookie: "cookie", secuencia: 2, ahora: inicio), .permitida)
        XCTAssertEqual(estado.abrir(
            codigo: nil, cookie: "cookie", nuevoToken: "ignorado",
            ahora: inicio), .existente(siguienteSecuencia: 3),
            "Recargar la página debe continuar en la próxima secuencia válida.")
        XCTAssertEqual(estado.autorizarAPI(
            cookie: "cookie", secuencia: 3,
            ahora: inicio.addingTimeInterval(61)), .expirada)
    }

    func testControlWebLimitaTasaYGeometriaAntesDeDecodificar() {
        var cuota = CuotaSolicitudesControlRemoto()
        XCTAssertTrue(cuota.admitir(ahora: 10, maximo: 2))
        XCTAssertTrue(cuota.admitir(ahora: 11, maximo: 2))
        XCTAssertFalse(cuota.admitir(ahora: 12, maximo: 2))
        XCTAssertTrue(cuota.admitir(ahora: 71, maximo: 2))

        XCTAssertTrue(PoliticaImagenControlRemoto.dimensionesAdmitidas(
            ancho: 8_064, alto: 6_048))
        XCTAssertFalse(PoliticaImagenControlRemoto.dimensionesAdmitidas(
            ancho: 20_000, alto: 100))
        XCTAssertFalse(PoliticaImagenControlRemoto.dimensionesAdmitidas(
            ancho: 10_000, alto: 10_000))
        XCTAssertFalse(PoliticaImagenControlRemoto.dimensionesAdmitidas(
            ancho: 0, alto: 1_000))
    }

    func testComparacionDelSecretoNoAceptaPrefijosNiLongitudesDistintas() {
        XCTAssertTrue(EstadoAutorizacionWebIPad.igualesConstante(
            "0123456789", "0123456789"))
        XCTAssertFalse(EstadoAutorizacionWebIPad.igualesConstante(
            "012345678", "0123456789"))
        XCTAssertFalse(EstadoAutorizacionWebIPad.igualesConstante(
            "1123456789", "0123456789"))
        XCTAssertFalse(EstadoAutorizacionWebIPad.igualesConstante(
            nil, "0123456789"))
    }

    func testParserControlWebAceptaSoloUnaPeticionHTTPAcotada() throws {
        let parser = ParserHTTPControlRemoto(maximoCabeceras: 1_024,
                                             maximoCuerpo: 16)
        let datos = Data(
            "POST /api/command?command=examen&seq=8 HTTP/1.1\r\nContent-Length: 3\r\n\r\nabc".utf8)
        let peticion = try parser.extraer(de: datos).get()
        XCTAssertEqual(peticion.metodo, "POST")
        XCTAssertEqual(peticion.ruta, "/api/command")
        XCTAssertEqual(peticion.consulta,
                       ["command": "examen", "seq": "8"])
        XCTAssertEqual(peticion.cuerpo, Data("abc".utf8))

        func error(_ texto: String) -> ErrorPeticionControlRemoto? {
            guard case .failure(let resultado) = parser.extraer(
                de: Data(texto.utf8)) else { return nil }
            return resultado
        }
        XCTAssertEqual(error(
            "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\nContent-Length: 0\r\n\r\n"),
            .invalida)
        XCTAssertEqual(error(
            "GET / HTTP/1.1\r\nHost: uno\r\nHost: dos\r\n\r\n"),
            .invalida)
        XCTAssertEqual(error(
            "GET /?seq=1&seq=2 HTTP/1.1\r\n\r\n"), .invalida)
        XCTAssertEqual(error(
            "POST / HTTP/1.1\r\nContent-Length: 1\r\n\r\nxy"),
            .invalida)
        XCTAssertEqual(error(
            "POST / HTTP/1.1\r\nContent-Length: 17\r\n\r\n"),
            .demasiadoGrande)
    }

    func testRespuestaControlWebUsaCRLFYDescartaInyeccion() {
        let cuerpo = Data("ok".utf8)
        let datos = ConstructorRespuestaHTTPControlRemoto.datos(
            codigo: 201, tipo: "application/json", cuerpo: cuerpo,
            cabecerasAdicionales: [
                "Set-Cookie": "CuadernUSesion=abc; HttpOnly; SameSite=Strict",
                "X-Mala": "x\r\nInjected: true",
            ])
        let texto = String(decoding: datos, as: UTF8.self)
        XCTAssertTrue(texto.hasPrefix("HTTP/1.1 201 Created\r\n"))
        XCTAssertTrue(texto.contains("Content-Length: 2\r\n"))
        XCTAssertTrue(texto.contains("X-Frame-Options: DENY\r\n"))
        XCTAssertTrue(texto.contains(
            "Set-Cookie: CuadernUSesion=abc; HttpOnly; SameSite=Strict\r\n"))
        XCTAssertFalse(texto.contains("Injected: true"))
        XCTAssertTrue(texto.hasSuffix("\r\n\r\nok"))
    }

    func testPlanificadorFusionaEventosYConservaElMasReciente() {
        var planificador = PlanificadorFramesTinta()
        planificador.solicitar(timestampEvento: 10)
        planificador.solicitar(timestampEvento: 12)
        planificador.solicitar(timestampEvento: 11)

        XCTAssertEqual(planificador.consumir(),
                       SolicitudFrameTinta(timestampEvento: 12))
        XCTAssertNil(planificador.consumir())
    }

    func testPlanificadorReponeFrameQueElGPUNoPudoAceptar() {
        var planificador = PlanificadorFramesTinta()
        planificador.solicitar(timestampEvento: 4.5)
        let solicitud = try! XCTUnwrap(planificador.consumir())
        planificador.reponer(solicitud)

        XCTAssertTrue(planificador.pendiente)
        XCTAssertEqual(planificador.consumir(), solicitud)
    }

    func testLatenciaUsaFinRealDeGPUYPercentiles() {
        var medidor = MedidorLatenciaTinta(capacidad: 4)
        XCTAssertTrue(medidor.registrar(timestampEvento: 1.000,
                                        tiempoCompletadoGPU: 1.004))
        XCTAssertTrue(medidor.registrar(timestampEvento: 2.000,
                                        tiempoCompletadoGPU: 2.006))
        XCTAssertTrue(medidor.registrar(timestampEvento: 3.000,
                                        tiempoCompletadoGPU: 3.008))
        XCTAssertTrue(medidor.registrar(timestampEvento: 4.000,
                                        tiempoCompletadoGPU: 4.020))

        let resumen = medidor.resumen
        XCTAssertEqual(resumen.cantidad, 4)
        XCTAssertEqual(resumen.p50, 7, accuracy: 0.001)
        XCTAssertGreaterThan(resumen.p95, resumen.p50)
        XCTAssertEqual(resumen.maxima, 20, accuracy: 0.001)
    }

    func testPDFConservaUnaPresentacionNormalEnUnaPagina() throws {
        let paginas = try PlanificadorPDFTinta.paginas(
            para: CGRect(x: 0, y: 0, width: 1920, height: 1080))
        XCTAssertEqual(paginas, [PaginaPDFTinta(regionDocumento:
            CGRect(x: 0, y: 0, width: 1920, height: 1080))])
    }

    func testPDFPaginaLienzosInfinitosVerticalesYHorizontales() throws {
        let vertical = try PlanificadorPDFTinta.paginas(
            para: CGRect(x: 0, y: 0, width: 1240, height: 10_000))
        XCTAssertEqual(vertical.count, 6)
        XCTAssertTrue(vertical.allSatisfy {
            $0.regionDocumento.width <= 1240 && $0.regionDocumento.height <= 1754
        })
        XCTAssertEqual(try XCTUnwrap(vertical.last).regionDocumento.maxY, 10_000,
                       accuracy: 0.001)

        let horizontal = try PlanificadorPDFTinta.paginas(
            para: CGRect(x: 0, y: 0, width: 10_000, height: 1240))
        XCTAssertEqual(horizontal.count, 6)
        XCTAssertTrue(horizontal.allSatisfy {
            $0.regionDocumento.width <= 1754 && $0.regionDocumento.height <= 1240
        })
        XCTAssertEqual(try XCTUnwrap(horizontal.last).regionDocumento.maxX, 10_000,
                       accuracy: 0.001)
    }

    func testPDFRechazaDimensionesInvalidasOUnNumeroDescontroladoDePaginas() {
        XCTAssertThrowsError(try PlanificadorPDFTinta.paginas(
            para: CGRect(x: 0, y: 0, width: 0, height: 100)))
        XCTAssertThrowsError(try PlanificadorPDFTinta.paginas(
            para: CGRect(x: 0, y: 0, width: 1_000_000, height: 1_000_000)))
    }

    func testBarraPersonalizableNormalizaPosicionesYSatelitesDuplicados() throws {
        var barra = ConfiguracionBarraIPad(
            ubicacion: .flotante,
            centroFlotanteX: -4,
            centroFlotanteY: 7,
            satelites: [
                HerramientaSateliteIPad(accion: .pluma, x: -2, y: 3),
                HerramientaSateliteIPad(accion: .pluma, x: 0.5, y: 0.5),
                HerramientaSateliteIPad(accion: .lazo, x: .nan, y: 0.5),
            ])

        barra.normalizar()

        XCTAssertEqual(barra.centroFlotanteX, 0.08, accuracy: 0.0001)
        XCTAssertEqual(barra.centroFlotanteY, 0.92, accuracy: 0.0001)
        XCTAssertEqual(barra.satelites.count, 1)
        let satelite = try XCTUnwrap(barra.satelites.first)
        XCTAssertEqual(satelite.accion, .pluma)
        XCTAssertEqual(satelite.x, 0.04, accuracy: 0.0001)
        XCTAssertEqual(satelite.y, 0.96, accuracy: 0.0001)
    }

    func testAparienciasCubrenSistemaClaroOscuroCuidadoVisualYSol() {
        XCTAssertEqual(Set(AparienciaTintaIPad.allCases.map(\.rawValue)),
                       Set(["sistema", "clara", "oscura", "cuidadoVisual", "sol"]))
        XCTAssertEqual(AparienciaTintaIPad.sistema.estiloInterfaz, .unspecified)
        XCTAssertEqual(AparienciaTintaIPad.clara.estiloInterfaz, .light)
        XCTAssertEqual(AparienciaTintaIPad.sol.estiloInterfaz, .light)
        XCTAssertEqual(AparienciaTintaIPad.oscura.estiloInterfaz, .dark)
        XCTAssertEqual(AparienciaTintaIPad.cuidadoVisual.estiloInterfaz, .dark)
        XCTAssertEqual(AparienciaTintaIPad.oscura.colorFiltroLienzo.cgColor.alpha,
                       0, accuracy: 0.0001)
        XCTAssertGreaterThan(AparienciaTintaIPad.cuidadoVisual.colorFiltroLienzo.cgColor.alpha,
                             0)
    }

    @MainActor
    private func esperarFIFO(_ emulador: EmuladorCalculadora,
                             timeout: TimeInterval = 5) async -> Bool {
        let limite = Date().addingTimeInterval(timeout)
        while emulador.entradasPendientes > 0, Date() < limite {
            // La captura del LCD vuelve a la cola principal. Dormir de forma
            // cooperativa deja que esa publicacion termine sin bloquear UIKit.
            try? await Task.sleep(nanoseconds: 5_000_000)
        }
        return emulador.entradasPendientes == 0
    }

    @MainActor
    func testLasDosCasioEmpaquetadasCarganYRespondenAlTeclado() async {
        for modelo in ModeloCalculadora.todos {
            // Ambos productos actuales muestran solo la calculadora y desactivan
            // el OCR de la cinta academica. El test replica esa configuracion real.
            let emulador = EmuladorCalculadora(modelo: modelo,
                                                decodificarTextoLCD: false)
            XCTAssertTrue(emulador.cargada,
                          "No se pudo cargar la ROM empaquetada de \(modelo.nombre)")
            XCTAssertGreaterThan(emulador.ancho, 0)
            XCTAssertGreaterThan(emulador.alto, 0)
            let pantallaInicial = emulador.pixeles
            emulador.pulsar(0x00)
            emulador.soltar(0x00)
            let fifoTerminada = await esperarFIFO(emulador)
            XCTAssertTrue(fifoTerminada,
                          "La FIFO de \(modelo.nombre) no termino dentro del plazo")
            XCTAssertEqual(emulador.historial, [0x00])
            XCTAssertTrue(emulador.pixeles.contains(where: { $0 != 0 }),
                           "El LCD de \(modelo.nombre) no respondio a una tecla")
            XCTAssertNotEqual(pantallaInicial, emulador.pixeles,
                               "El LCD de \(modelo.nombre) no cambio al pulsar")
        }
    }

    @MainActor
    func testRafagaSwiftConservaOrdenYResultadoDeAmbosModelos() async {
        let secuencia: [UInt8] = [
            0x00, 0x10, 0x20, 0x01, 0x11, 0x21,
            0x02, 0x12, 0x22, 0x64,
        ]

        for modelo in ModeloCalculadora.todos {
            let rafaga = EmuladorCalculadora(modelo: modelo,
                                              decodificarTextoLCD: false)
            for codigo in secuencia {
                rafaga.pulsar(codigo)
                rafaga.soltar(codigo)
            }
            let rafagaTerminada = await esperarFIFO(rafaga, timeout: 10)
            XCTAssertTrue(rafagaTerminada,
                          "La rafaga de \(modelo.nombre) no termino dentro del plazo")
            XCTAssertEqual(rafaga.historial, secuencia,
                           "Swift omitio o reordeno entradas de \(modelo.nombre)")

            let referencia = EmuladorCalculadora(modelo: modelo,
                                                  decodificarTextoLCD: false)
            for codigo in secuencia {
                referencia.pulsar(codigo)
                referencia.soltar(codigo)
                let referenciaTerminada = await esperarFIFO(referencia)
                XCTAssertTrue(referenciaTerminada,
                              "La referencia de \(modelo.nombre) no termino")
            }

            XCTAssertEqual(rafaga.pixeles, referencia.pixeles,
                           "La FIFO rapida produjo otro LCD en \(modelo.nombre)")
            XCTAssertEqual(rafaga.indicadores, referencia.indicadores,
                           "La FIFO rapida produjo otros indicadores en \(modelo.nombre)")
        }
    }
}
