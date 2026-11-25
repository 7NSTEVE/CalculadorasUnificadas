import XCTest
@testable import CuadernUEstudio

final class OrganizacionBibliotecaIPadTests: XCTestCase {
    private func base() throws -> URL {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("org-biblioteca-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        return url
    }

    func testCarpetasJerarquicasReabren() throws {
        let carpeta = try base()
        let padre: CarpetaBibliotecaIPad
        let hija: CarpetaBibliotecaIPad
        do {
            let repo = RepositorioOrganizacionBibliotecaIPad(base: carpeta)
            padre = try repo.crearCarpeta(nombre: "Electrónica")
            hija = try repo.crearCarpeta(nombre: "Circuitos II", padreId: padre.id)
            try repo.moverCuaderno("abc", a: hija.id)
            repo.favorito("abc", true)
        }
        let reabierto = RepositorioOrganizacionBibliotecaIPad(base: carpeta)
        XCTAssertEqual(reabierto.ruta(carpetaId: hija.id).map(\.nombre), ["Electrónica", "Circuitos II"])
        XCTAssertEqual(reabierto.metadata(cuadernoId: "abc").carpetaId, hija.id)
        XCTAssertTrue(reabierto.metadata(cuadernoId: "abc").favorito)
    }

    func testEvitaCicloDeCarpetas() throws {
        let repo = RepositorioOrganizacionBibliotecaIPad(base: try base())
        let a = try repo.crearCarpeta(nombre: "A")
        let b = try repo.crearCarpeta(nombre: "B", padreId: a.id)
        XCTAssertThrowsError(try repo.moverCarpeta(id: a.id, a: b.id)) {
            XCTAssertEqual($0 as? ErrorOrganizacionBibliotecaIPad, .jerarquiaCircular)
        }
    }

    func testNoEliminaCarpetaConContenido() throws {
        let repo = RepositorioOrganizacionBibliotecaIPad(base: try base())
        let carpeta = try repo.crearCarpeta(nombre: "Con cuaderno")
        try repo.moverCuaderno("doc", a: carpeta.id)
        XCTAssertThrowsError(try repo.eliminarCarpeta(carpeta.id)) {
            XCTAssertEqual($0 as? ErrorOrganizacionBibliotecaIPad, .carpetaNoVacia)
        }
    }

    func testSimboloDesconocidoCaeEnFolderSeguro() throws {
        let repo = RepositorioOrganizacionBibliotecaIPad(base: try base())
        let carpeta = try repo.crearCarpeta(nombre: "X", simboloSF: "simbolo.inventado")
        XCTAssertEqual(carpeta.simboloSF, "folder.fill")
    }

    func testMarcadoresAisladosPorDocumentoYReapertura() throws {
        let base = try base()
        let a = DocumentoPersistenteID(), b = DocumentoPersistenteID()
        let repo = RepositorioMarcadoresDocumentoIPad(base: base)
        XCTAssertTrue(repo.guardar(.init(documentoId: a, pagina: 4, titulo: "Gauss")))
        XCTAssertTrue(repo.guardar(.init(documentoId: b, pagina: 1, titulo: "Faraday")))
        XCTAssertEqual(repo.marcadores(documentoId: a).map(\.titulo), ["Gauss"])
        XCTAssertEqual(RepositorioMarcadoresDocumentoIPad(base: base)
            .marcadores(documentoId: b).map(\.titulo), ["Faraday"])
    }
}
