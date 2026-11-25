import XCTest
@testable import CuadernUEstudio

final class BibliotecaElementosReutilizablesIPadTests: XCTestCase {
    private func carpetaTemporal() -> URL {
        FileManager.default.temporaryDirectory
            .appendingPathComponent("cuadernu-elementos-\(UUID().uuidString)", isDirectory: true)
    }

    private func biblioteca(en base: URL) -> BibliotecaElementosReutilizablesIPad {
        let recursos = base.appendingPathComponent("Recursos", isDirectory: true)
        return BibliotecaElementosReutilizablesIPad(
            base: base,
            almacen: AlmacenRecursosElementosIPad(base: recursos))
    }

    func testGuardarReabrirYLeerPayload() throws {
        let base = carpetaTemporal()
        defer { try? FileManager.default.removeItem(at: base) }
        let datos = Data("circuito-rlc".utf8)

        let primera = biblioteca(en: base)
        let coleccion = try primera.crearColeccion(nombre: "Circuitos")
        let elemento = try primera.guardarElemento(
            coleccionId: coleccion.id,
            tipo: .circuito,
            nombre: "RLC serie",
            etiquetas: ["RLC", "impedancia"],
            payload: datos,
            anchoNatural: 420,
            altoNatural: 180)

        XCTAssertEqual(try primera.payload(elementoId: elemento.id), datos)

        let reabierta = biblioteca(en: base)
        XCTAssertEqual(reabierta.instantanea().colecciones.count, 1)
        XCTAssertEqual(reabierta.instantanea().elementos.map(\.id), [elemento.id])
        XCTAssertEqual(try reabierta.payload(elementoId: elemento.id), datos)
    }

    func testRecursosIgualesCompartenHashSinPerderElementos() throws {
        let base = carpetaTemporal()
        defer { try? FileManager.default.removeItem(at: base) }
        let repo = biblioteca(en: base)
        let coleccion = try repo.crearColeccion(nombre: "Fórmulas")
        let payload = Data("V=IR".utf8)

        let a = try repo.guardarElemento(
            coleccionId: coleccion.id, tipo: .formula, nombre: "Ohm",
            payload: payload, anchoNatural: 100, altoNatural: 40)
        let b = try repo.guardarElemento(
            coleccionId: coleccion.id, tipo: .formula, nombre: "Ley de Ohm",
            payload: payload, anchoNatural: 100, altoNatural: 40)

        XCTAssertNotEqual(a.id, b.id)
        XCTAssertEqual(a.recurso.hashSHA256, b.recurso.hashSHA256)
        XCTAssertEqual(repo.instantanea().elementos.count, 2)
    }

    func testBusquedaExigeTodosLosTerminosYPriorizaNombre() throws {
        let base = carpetaTemporal()
        defer { try? FileManager.default.removeItem(at: base) }
        let repo = biblioteca(en: base)
        let coleccion = try repo.crearColeccion(nombre: "Electrónica")

        _ = try repo.guardarElemento(
            coleccionId: coleccion.id, tipo: .circuito,
            nombre: "Amplificador operacional inversor",
            detalle: "ganancia negativa",
            etiquetas: ["opamp"], payload: Data([1]),
            anchoNatural: 200, altoNatural: 100)
        _ = try repo.guardarElemento(
            coleccionId: coleccion.id, tipo: .circuito,
            nombre: "Amplificador no inversor",
            detalle: "operacional ideal",
            etiquetas: ["opamp"], payload: Data([2]),
            anchoNatural: 200, altoNatural: 100)

        let resultados = repo.buscar("amplificador inversor")
        XCTAssertEqual(resultados.count, 2)
        XCTAssertEqual(resultados.first?.nombre, "Amplificador operacional inversor")
        XCTAssertTrue(repo.buscar("amplificador mosfet").isEmpty)
    }

    func testFavoritosYUsosOrdenanConsultaVacia() throws {
        let base = carpetaTemporal()
        defer { try? FileManager.default.removeItem(at: base) }
        let repo = biblioteca(en: base)
        let coleccion = try repo.crearColeccion(nombre: "Frecuentes")

        let normal = try repo.guardarElemento(
            coleccionId: coleccion.id, tipo: .figura, nombre: "Triángulo",
            payload: Data([1]), anchoNatural: 40, altoNatural: 40)
        let favorito = try repo.guardarElemento(
            coleccionId: coleccion.id, tipo: .figura, nombre: "Flecha",
            payload: Data([2]), anchoNatural: 40, altoNatural: 40)
        repo.registrarUso(elementoId: normal.id)
        repo.registrarUso(elementoId: normal.id)
        repo.favorito(elementoId: favorito.id, valor: true)

        XCTAssertEqual(repo.buscar("").first?.id, favorito.id)
    }

    func testArchivarColeccionLaOcultaSinBorrarDatos() throws {
        let base = carpetaTemporal()
        defer { try? FileManager.default.removeItem(at: base) }
        let repo = biblioteca(en: base)
        let coleccion = try repo.crearColeccion(nombre: "Viejos")
        _ = try repo.guardarElemento(
            coleccionId: coleccion.id, tipo: .texto, nombre: "Ejemplo",
            payload: Data([9]), anchoNatural: 10, altoNatural: 10)

        repo.archivarColeccion(id: coleccion.id)

        XCTAssertTrue(repo.buscar("").isEmpty)
        XCTAssertEqual(repo.instantanea().elementos.count, 1)
        XCTAssertTrue(repo.instantanea().colecciones.first?.archivada == true)
    }

    func testDimensionesInvalidasSeRechazanAntesDePersistir() throws {
        let base = carpetaTemporal()
        defer { try? FileManager.default.removeItem(at: base) }
        let repo = biblioteca(en: base)
        let coleccion = try repo.crearColeccion(nombre: "Prueba")

        XCTAssertThrowsError(try repo.guardarElemento(
            coleccionId: coleccion.id, tipo: .mixto, nombre: "Inválido",
            payload: Data([1]), anchoNatural: 0, altoNatural: 10))
        XCTAssertTrue(repo.instantanea().elementos.isEmpty)
    }
}
