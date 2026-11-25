import XCTest
@testable import CuadernUEstudio

final class ComentariosDocumentoIPadTests: XCTestCase {
    private func base() -> URL {
        FileManager.default.temporaryDirectory
            .appendingPathComponent("cuadernu-comentarios-\(UUID().uuidString)", isDirectory: true)
    }

    func testCrearResponderResolverYReabrir() throws {
        let base = base(); defer { try? FileManager.default.removeItem(at: base) }
        let documento = DocumentoPersistenteID()
        let repo = RepositorioComentariosDocumentoIPad(base: base)
        let anclaje = AnclajeComentarioDocumentoIPad(
            documentoId: documento, pagina: 2,
            regionNormalizada: .init(x: 0.1, y: 0.2, ancho: 0.3, alto: 0.1),
            contexto: "Ley de Ohm")
        let hilo = try repo.crearHilo(
            anclaje: anclaje, autorId: "local", autorNombre: "Yo",
            texto: "Revisar el signo")
        _ = try repo.responder(
            hiloId: hilo.id, documentoId: documento,
            autorId: "prof", autorNombre: "Profesor", texto: "Correcto: use convención pasiva")
        try repo.resolver(hiloId: hilo.id, documentoId: documento, valor: true)

        let reabierto = RepositorioComentariosDocumentoIPad(base: base)
        let guardado = try XCTUnwrap(reabierto.hilos(documentoId: documento).first)
        XCTAssertTrue(guardado.resuelto)
        XCTAssertEqual(guardado.mensajes.count, 2)
        XCTAssertEqual(guardado.anclaje.pagina, 2)
        XCTAssertEqual(guardado.anclaje.contexto, "Ley de Ohm")
    }

    func testDosDocumentosNoCruzanThreads() throws {
        let base = base(); defer { try? FileManager.default.removeItem(at: base) }
        let a = DocumentoPersistenteID(), b = DocumentoPersistenteID()
        let repo = RepositorioComentariosDocumentoIPad(base: base)
        _ = try repo.crearHilo(
            anclaje: .init(documentoId: a, contexto: "A"),
            autorId: "u", autorNombre: "U", texto: "Solo A")
        _ = try repo.crearHilo(
            anclaje: .init(documentoId: b, contexto: "B"),
            autorId: "u", autorNombre: "U", texto: "Solo B")

        XCTAssertEqual(repo.hilos(documentoId: a).first?.mensajes.first?.texto, "Solo A")
        XCTAssertEqual(repo.hilos(documentoId: b).first?.mensajes.first?.texto, "Solo B")
    }

    func testBuscarExcluyeResueltosPorDefecto() throws {
        let base = base(); defer { try? FileManager.default.removeItem(at: base) }
        let documento = DocumentoPersistenteID()
        let repo = RepositorioComentariosDocumentoIPad(base: base)
        let activo = try repo.crearHilo(
            anclaje: .init(documentoId: documento, contexto: "KVL"),
            autorId: "u", autorNombre: "U", texto: "Revisar malla izquierda")
        let resuelto = try repo.crearHilo(
            anclaje: .init(documentoId: documento, contexto: "KCL"),
            autorId: "u", autorNombre: "U", texto: "Nodo superior")
        try repo.resolver(hiloId: resuelto.id, documentoId: documento, valor: true)

        XCTAssertEqual(repo.buscar("malla", documentoId: documento).map(\.id), [activo.id])
        XCTAssertTrue(repo.buscar("nodo", documentoId: documento).isEmpty)
        XCTAssertEqual(repo.buscar("nodo", documentoId: documento,
                                    incluirResueltos: true).map(\.id), [resuelto.id])
    }

    func testEditarMensajeMarcaFechaDeEdicion() throws {
        let base = base(); defer { try? FileManager.default.removeItem(at: base) }
        let documento = DocumentoPersistenteID()
        let repo = RepositorioComentariosDocumentoIPad(base: base)
        let hilo = try repo.crearHilo(
            anclaje: .init(documentoId: documento),
            autorId: "u", autorNombre: "U", texto: "texto inicial")
        let mensaje = try XCTUnwrap(hilo.mensajes.first)
        try repo.editarMensaje(
            mensajeId: mensaje.id, hiloId: hilo.id,
            documentoId: documento, texto: "texto corregido")

        let editado = try XCTUnwrap(repo.hilos(documentoId: documento).first?.mensajes.first)
        XCTAssertEqual(editado.texto, "texto corregido")
        XCTAssertNotNil(editado.editado)
    }

    func testRegionFueraDePaginaSeRechaza() {
        let base = base(); defer { try? FileManager.default.removeItem(at: base) }
        let repo = RepositorioComentariosDocumentoIPad(base: base)
        let anclaje = AnclajeComentarioDocumentoIPad(
            documentoId: DocumentoPersistenteID(),
            regionNormalizada: .init(x: 0.9, y: 0.1, ancho: 0.2, alto: 0.1))
        XCTAssertThrowsError(try repo.crearHilo(
            anclaje: anclaje, autorId: "u", autorNombre: "U", texto: "x"))
    }
}
