import XCTest
@testable import CuadernUEstudio

final class EnlacesYBacklinksIPadTests: XCTestCase {
    func testDetectaHTTPYWWW() {
        let texto = "Vea https://example.com/a?x=1 y también www.goodnotes.com."
        let enlaces = DetectorEnlacesAutomaticosIPad.detectar(en: texto)
        XCTAssertEqual(enlaces.count, 2)
        XCTAssertEqual(enlaces[0].url.absoluteString, "https://example.com/a?x=1")
        XCTAssertEqual(enlaces[1].url.absoluteString, "https://www.goodnotes.com")
    }

    func testNoIncluyePuntuacionFinal() {
        let enlace = DetectorEnlacesAutomaticosIPad.detectar(
            en: "Fuente: https://example.com/doc.pdf).")
        XCTAssertEqual(enlace.first?.url.absoluteString, "https://example.com/doc.pdf")
    }

    func testRangoUTF16ReconstruyeTexto() {
        let texto = "µ → https://example.com"
        let enlace = try! XCTUnwrap(DetectorEnlacesAutomaticosIPad.detectar(en: texto).first)
        let ns = texto as NSString
        let rango = NSRange(location: enlace.rango.localizacionUTF16,
                            length: enlace.rango.longitudUTF16)
        XCTAssertEqual(ns.substring(with: rango), "https://example.com")
    }

    func testCodecTintaRoundTripConDestinoEstable() throws {
        let documento = DocumentoPersistenteID()
        let frame = UUID()
        let destino = DestinoEnlaceIPad.documento(documento, pagina: 12, frameId: frame)

        let url = try CodecEnlacesInternosIPad.crearURL(para: destino)
        XCTAssertEqual(url.scheme, "tinta")
        XCTAssertEqual(url.host, "abrir")
        XCTAssertEqual(try CodecEnlacesInternosIPad.decodificar(url), destino)
    }

    func testCodecRechazaVersionDesconocidaYParametrosDuplicados() throws {
        let documento = DocumentoPersistenteID()
        let versionDesconocida = "tinta://abrir?v=2&tipo=documento&clave=\(documento)&documento=\(documento)"
        XCTAssertThrowsError(try CodecEnlacesInternosIPad.decodificar(
            XCTUnwrap(URL(string: versionDesconocida)))) { error in
            XCTAssertEqual(error as? ErrorCodecEnlaceInternoIPad, .versionNoSoportada)
        }

        let duplicado = "tinta://abrir?v=1&v=1&tipo=documento&clave=\(documento)&documento=\(documento)"
        XCTAssertThrowsError(try CodecEnlacesInternosIPad.decodificar(
            XCTUnwrap(URL(string: duplicado)))) { error in
            XCTAssertEqual(error as? ErrorCodecEnlaceInternoIPad, .parametrosDuplicados)
        }
    }

    func testCodecRechazaPaginaNegativaYDocumentoInconsistente() throws {
        let documento = DocumentoPersistenteID()
        let negativo = "tinta://abrir?v=1&tipo=documento&clave=\(documento)&documento=\(documento)&pagina=-1"
        XCTAssertThrowsError(try CodecEnlacesInternosIPad.decodificar(
            XCTUnwrap(URL(string: negativo)))) { error in
            XCTAssertEqual(error as? ErrorCodecEnlaceInternoIPad, .paginaInvalida)
        }

        let otro = DocumentoPersistenteID()
        let inconsistente = "tinta://abrir?v=1&tipo=documento&clave=\(documento)&documento=\(otro)"
        XCTAssertThrowsError(try CodecEnlacesInternosIPad.decodificar(
            XCTUnwrap(URL(string: inconsistente)))) { error in
            XCTAssertEqual(error as? ErrorCodecEnlaceInternoIPad, .destinoInconsistente)
        }
    }

    func testDetectorReconoceTintaValidoYDescartaCorrupto() throws {
        let documento = DocumentoPersistenteID()
        let enlace = try CodecEnlacesInternosIPad.crearURL(
            para: .documento(documento, pagina: 4))
        let texto = "Vea \(enlace.absoluteString) y tinta://abrir?v=99&tipo=documento&clave=x"
        let detectados = DetectorEnlacesAutomaticosIPad.detectar(en: texto)

        XCTAssertEqual(detectados.count, 1)
        XCTAssertEqual(detectados.first?.url, enlace)
    }

    func testDetectorConservaTintaLegacyDuranteMigracion() throws {
        let legacy = DestinoEnlaceTinta(
            documentoId: 987_654,
            elementoId: "texto-1",
            bloqueId: nil,
            x: 120,
            y: 320,
            zoom: 1.4,
            segundoAudio: nil,
            titulo: "Nota anterior")
        let url = try XCTUnwrap(legacy.enlace())
        let detectados = DetectorEnlacesAutomaticosIPad.detectar(
            en: "Referencia histórica: \(url.absoluteString)")

        XCTAssertEqual(detectados.count, 1)
        XCTAssertEqual(detectados.first?.url, url)
        XCTAssertNotNil(DestinoEnlaceTinta.leer(url))
    }

    func testBacklinkSeConsultaPorDestinoEstable() {
        let repo = RepositorioBacklinksIPad(base: temporal())
        let origen = DocumentoPersistenteID(UUID())
        let problemaId = UUID()
        let destino = DestinoEnlaceIPad.problema(problemaId)
        let backlink = BacklinkCuadernUIPad(destino: destino,
                                            documentoOrigen: origen,
                                            etiqueta: "Ejercicio relacionado")
        XCTAssertTrue(repo.upsert(backlink))
        XCTAssertEqual(repo.backlinks(hacia: destino).map(\.id), [backlink.id])
    }

    func testUpsertSemanticoNoDuplicaBacklinkTrasReindexar() {
        let repo = RepositorioBacklinksIPad(base: temporal())
        let origen = DocumentoPersistenteID()
        let destino = DestinoEnlaceIPad.documento(DocumentoPersistenteID(), pagina: 3)
        let primero = BacklinkCuadernUIPad(
            destino: destino, documentoOrigen: origen, paginaOrigen: 1,
            etiqueta: "Primero", contexto: "viejo")
        let segundo = BacklinkCuadernUIPad(
            destino: destino, documentoOrigen: origen, paginaOrigen: 1,
            etiqueta: "Actualizado", contexto: "nuevo")

        XCTAssertTrue(repo.upsert(primero))
        XCTAssertTrue(repo.upsert(segundo))

        let guardados = repo.salientes(desde: origen)
        XCTAssertEqual(guardados.count, 1)
        XCTAssertEqual(guardados[0].id, primero.id)
        XCTAssertEqual(guardados[0].etiqueta, "Actualizado")
        XCTAssertEqual(guardados[0].contexto, "nuevo")
    }

    func testBacklinksPersistenFechaISO8601YReabren() {
        let base = temporal()
        let origen = DocumentoPersistenteID()
        let destino = DestinoEnlaceIPad.documento(DocumentoPersistenteID(), pagina: 8)
        let fecha = Date(timeIntervalSince1970: 1_700_000_000)
        let backlink = BacklinkCuadernUIPad(
            destino: destino, documentoOrigen: origen,
            etiqueta: "Persistente", contexto: "round-trip", fecha: fecha)

        XCTAssertTrue(RepositorioBacklinksIPad(base: base).upsert(backlink))
        let reabierto = RepositorioBacklinksIPad(base: base)
        let guardados = reabierto.salientes(desde: origen)

        XCTAssertEqual(guardados.count, 1)
        XCTAssertEqual(guardados[0].id, backlink.id)
        XCTAssertEqual(guardados[0].destino, destino)
        XCTAssertEqual(guardados[0].fecha.timeIntervalSince1970,
                       fecha.timeIntervalSince1970, accuracy: 0.001)
    }

    func testReemplazarSalientesNoBorraOtroDocumento() {
        let repo = RepositorioBacklinksIPad(base: temporal())
        let a = DocumentoPersistenteID(UUID()), b = DocumentoPersistenteID(UUID())
        let destino = DestinoEnlaceIPad.fuente("dorf-cap-2")
        let deB = BacklinkCuadernUIPad(destino: destino, documentoOrigen: b)
        XCTAssertTrue(repo.upsert(deB))
        let deA = BacklinkCuadernUIPad(destino: destino, documentoOrigen: a)
        XCTAssertTrue(repo.reemplazarSalientes(de: a, con: [deA]))
        XCTAssertEqual(repo.salientes(desde: a).map(\.id), [deA.id])
        XCTAssertEqual(repo.salientes(desde: b).map(\.id), [deB.id])
    }

    func testReemplazarSalientesEliminaResiduosYDeduplicaEntrada() {
        let repo = RepositorioBacklinksIPad(base: temporal())
        let origen = DocumentoPersistenteID()
        let destinoViejo = DestinoEnlaceIPad.documento(DocumentoPersistenteID())
        let destinoNuevo = DestinoEnlaceIPad.documento(DocumentoPersistenteID(), pagina: 2)
        XCTAssertTrue(repo.upsert(.init(destino: destinoViejo, documentoOrigen: origen)))

        let a = BacklinkCuadernUIPad(destino: destinoNuevo, documentoOrigen: origen,
                                    paginaOrigen: 7, etiqueta: "A")
        let b = BacklinkCuadernUIPad(destino: destinoNuevo, documentoOrigen: origen,
                                    paginaOrigen: 7, etiqueta: "B")
        XCTAssertTrue(repo.reemplazarSalientes(de: origen, con: [a, b]))

        let guardados = repo.salientes(desde: origen)
        XCTAssertEqual(guardados.count, 1)
        XCTAssertEqual(guardados[0].destino, destinoNuevo)
        XCTAssertEqual(guardados[0].etiqueta, "B")
    }

    func testDestinoDocumentoUsaUUIDDurable() {
        let documento = DocumentoPersistenteID(UUID())
        let destino = DestinoEnlaceIPad.documento(documento, pagina: 7)
        XCTAssertEqual(destino.documentoId, documento)
        XCTAssertEqual(destino.claveBacklink, "documento:\(documento.description)")
        XCTAssertEqual(destino.pagina, 7)
    }

    private func temporal() -> URL {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("CuadernU-Backlinks-\(UUID().uuidString)", isDirectory: true)
        try? FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        addTeardownBlock { try? FileManager.default.removeItem(at: url) }
        return url
    }
}
