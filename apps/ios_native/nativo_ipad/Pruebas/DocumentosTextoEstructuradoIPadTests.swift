import XCTest
@testable import CuadernUEstudio

final class DocumentosTextoEstructuradoIPadTests: XCTestCase {
    private func temporal() -> URL {
        FileManager.default.temporaryDirectory
            .appendingPathComponent("cuadernu-textdoc-\(UUID().uuidString)", isDirectory: true)
    }

    func testCrearGuardarReabrirConservaIdentidadYBloques() throws {
        let base = temporal(); defer { try? FileManager.default.removeItem(at: base) }
        let repo = RepositorioDocumentosTextoIPad(base: base)
        var doc = try repo.crear(titulo: "Circuitos")
        doc.bloques = [
            BloqueDocumentoTextoIPad(tipo: .titulo1, texto: "Ley de Ohm"),
            BloqueDocumentoTextoIPad(tipo: .parrafo, texto: "V = IR"),
            BloqueDocumentoTextoIPad(tipo: .tarea, texto: "Resolver ejemplo")
        ]
        try repo.guardar(doc)
        let abierto = try repo.cargar(doc.id)
        XCTAssertEqual(abierto.id, doc.id)
        XCTAssertEqual(abierto.titulo, "Circuitos")
        XCTAssertEqual(abierto.bloques.count, 3)
        XCTAssertEqual(abierto.bloques[1].texto, "V = IR")
    }

    func testMoverEliminarYToggleTareaIncrementanRevision() {
        var doc = DocumentoTextoEstructuradoIPad(titulo: "X")
        let a = BloqueDocumentoTextoIPad(tipo: .parrafo, texto: "A")
        let b = BloqueDocumentoTextoIPad(tipo: .tarea, texto: "B")
        doc.bloques = [a, b]
        let r0 = doc.revision
        XCTAssertTrue(doc.mover(desde: 1, hacia: 0))
        XCTAssertGreaterThan(doc.revision, r0)
        XCTAssertTrue(doc.alternarTarea(b.id))
        XCTAssertTrue(doc.bloques.first?.completado == true)
        XCTAssertTrue(doc.eliminar(a.id))
        XCTAssertEqual(doc.bloques.count, 1)
    }

    func testNuncaQuedaSinBloquesAlEliminarUltimo() {
        var doc = DocumentoTextoEstructuradoIPad(titulo: "X")
        let id = doc.bloques[0].id
        XCTAssertTrue(doc.eliminar(id))
        XCTAssertEqual(doc.bloques.count, 1)
        XCTAssertEqual(doc.bloques[0].tipo, .parrafo)
    }

    func testTablaNormalizaLimites() {
        var tabla = TablaDocumentoTextoIPad(filas: 2, columnas: 3)
        tabla.filas.append([])
        tabla.normalizar()
        XCTAssertEqual(tabla.filas.count, 3)
        XCTAssertEqual(tabla.columnas, 3)
        XCTAssertTrue(tabla.filas.allSatisfy { $0.count == 3 })
    }

    func testMarkdownExportaSemantica() {
        var doc = DocumentoTextoEstructuradoIPad(titulo: "Prueba")
        var tarea = BloqueDocumentoTextoIPad(tipo: .tarea, texto: "Repasar")
        tarea.completado = true
        var codigo = BloqueDocumentoTextoIPad(tipo: .codigo, texto: "int x = 1;")
        codigo.lenguajeCodigo = "cpp"
        doc.bloques = [
            BloqueDocumentoTextoIPad(tipo: .titulo1, texto: "Tema"),
            tarea,
            BloqueDocumentoTextoIPad(tipo: .ecuacion, texto: "V=IR"),
            codigo
        ]
        let md = ExportadorDocumentoTextoIPad.markdown(doc)
        XCTAssertTrue(md.contains("# Tema"))
        XCTAssertTrue(md.contains("- [x] Repasar"))
        XCTAssertTrue(md.contains("V=IR"))
        XCTAssertTrue(md.contains("```cpp"))
    }

    func testListarOrdenaPorModificacion() throws {
        let base = temporal(); defer { try? FileManager.default.removeItem(at: base) }
        let repo = RepositorioDocumentosTextoIPad(base: base)
        var a = try repo.crear(titulo: "A")
        var b = try repo.crear(titulo: "B")
        a.modificado = Date(timeIntervalSince1970: 100)
        b.modificado = Date(timeIntervalSince1970: 200)
        try repo.guardar(a); try repo.guardar(b)
        XCTAssertEqual(repo.listar().map(\.titulo), ["B", "A"])
    }
}