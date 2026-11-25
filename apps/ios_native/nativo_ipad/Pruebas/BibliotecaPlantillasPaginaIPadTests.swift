import XCTest
@testable import CuadernUEstudio

final class BibliotecaPlantillasPaginaIPadTests: XCTestCase {
    private func temporal() -> URL {
        FileManager.default.temporaryDirectory
            .appendingPathComponent("cuadernu-plantillas-\(UUID().uuidString)", isDirectory: true)
    }

    func testPlantillasIncorporadasTienenIdentidadEstable() {
        let idsA = BibliotecaPlantillasPaginaIPad.incorporadas.map(\.id)
        let idsB = BibliotecaPlantillasPaginaIPad.incorporadas.map(\.id)
        XCTAssertEqual(idsA, idsB)
        XCTAssertEqual(Set(idsA).count, idsA.count)
        XCTAssertTrue(BibliotecaPlantillasPaginaIPad.incorporadas.contains { $0.nombre == "Semilog Bode" })
        XCTAssertTrue(BibliotecaPlantillasPaginaIPad.incorporadas.contains { $0.nombre == "Cornell" })
    }

    func testCrearFavoritoYReabrir() throws {
        let base = temporal(); defer { try? FileManager.default.removeItem(at: base) }
        let repo = BibliotecaPlantillasPaginaIPad(base: base)
        var estilo = EstiloPlantillaPaginaIPad()
        estilo.separacionPuntos = 20
        let creada = try repo.crear(PlantillaPaginaIPad(
            nombre: "Mi papel", categoria: "Personal", pauta: .lineas,
            estilo: estilo, etiquetas: ["clase"])); repo.favorito(creada.id, valor: true)

        let reabierto = BibliotecaPlantillasPaginaIPad(base: base)
        XCTAssertTrue(reabierto.esFavorita(creada.id))
        XCTAssertEqual(reabierto.todas().first?.id, creada.id)
    }

    func testExportarImportarCreaNuevaIdentidad() throws {
        let base = temporal(); defer { try? FileManager.default.removeItem(at: base) }
        let repo = BibliotecaPlantillasPaginaIPad(base: base)
        let original = BibliotecaPlantillasPaginaIPad.incorporadas.first { $0.pauta == .polar }!
        let datos = try XCTUnwrap(repo.exportar(original.id))
        let importada = try repo.importar(datos)

        XCTAssertNotEqual(importada.id, original.id)
        XCTAssertEqual(importada.pauta, .polar)
        XCTAssertFalse(importada.incorporada)
    }

    func testBusquedaTecnicaPorEtiquetas() {
        let base = temporal(); defer { try? FileManager.default.removeItem(at: base) }
        let repo = BibliotecaPlantillasPaginaIPad(base: base)
        let resultados = repo.buscar("bode frecuencia")
        XCTAssertEqual(resultados.map(\.nombre), ["Semilog Bode"])
    }

    func testOcultarIncorporadaNoLaBorra() {
        let base = temporal(); defer { try? FileManager.default.removeItem(at: base) }
        let repo = BibliotecaPlantillasPaginaIPad(base: base)
        let id = BibliotecaPlantillasPaginaIPad.incorporadas[0].id
        repo.ocultar(id, valor: true)
        XCTAssertFalse(repo.todas().contains { $0.id == id })
        XCTAssertTrue(repo.todas(incluirOcultas: true).contains { $0.id == id })
    }

    func testDimensionesInvalidasSeRechazan() {
        let base = temporal(); defer { try? FileManager.default.removeItem(at: base) }
        let repo = BibliotecaPlantillasPaginaIPad(base: base)
        let invalida = PlantillaPaginaIPad(
            nombre: "Mala", pauta: .cuadricula,
            tamano: .init(anchoPuntos: 0, altoPuntos: 800))
        XCTAssertThrowsError(try repo.crear(invalida))
    }
}
