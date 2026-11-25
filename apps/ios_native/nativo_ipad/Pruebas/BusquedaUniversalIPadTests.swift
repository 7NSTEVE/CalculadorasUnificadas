import XCTest
@testable import CuadernUEstudio

final class BusquedaUniversalIPadTests: XCTestCase {
    func testTituloExactoSuperaCoincidenciaSoloEnContenido() {
        let indice = IndiceBusquedaUniversalIPad()
        indice.reemplazar([
            entrada("titulo", .formula, titulo: "Kirchhoff", contenido: "Ley"),
            entrada("contenido", .texto, titulo: "Clase", contenido: "Aplicamos Kirchhoff aquí")
        ])

        let resultados = indice.buscar(.init(texto: "Kirchhoff"))

        XCTAssertEqual(resultados.first?.entrada.claveEstable, "titulo")
        XCTAssertGreaterThan(resultados[0].puntuacion, resultados[1].puntuacion)
    }

    func testConsultaDeVariosTerminosUsaSemanticaAND() {
        let indice = IndiceBusquedaUniversalIPad()
        indice.reemplazar([
            entrada("completo", .problema, titulo: "Circuito RC", contenido: "frecuencia de corte"),
            entrada("parcial", .problema, titulo: "Circuito RC", contenido: "respuesta temporal")
        ])

        let resultados = indice.buscar(.init(texto: "circuito corte"))

        XCTAssertEqual(resultados.map(\.entrada.claveEstable), ["completo"])
    }

    func testFiltraPorCategoriaYCurso() {
        let cursoA = UUID(), cursoB = UUID()
        let indice = IndiceBusquedaUniversalIPad()
        indice.reemplazar([
            entrada("formula-a", .formula, titulo: "Ohm", contenido: "V=IR", cursoId: cursoA),
            entrada("problema-a", .problema, titulo: "Ohm", contenido: "calcule R", cursoId: cursoA),
            entrada("formula-b", .formula, titulo: "Ohm", contenido: "V=IR", cursoId: cursoB)
        ])
        var consulta = ConsultaBusquedaUniversalIPad(texto: "Ohm")
        consulta.categorias = [.formula]
        consulta.cursoId = cursoA

        let resultados = indice.buscar(consulta)

        XCTAssertEqual(resultados.map(\.entrada.claveEstable), ["formula-a"])
    }

    func testUpsertReemplazaMismaClaveEnLugarDeDuplicar() {
        let indice = IndiceBusquedaUniversalIPad()
        indice.upsert([entrada("x", .texto, titulo: "Viejo", contenido: "uno")])
        indice.upsert([entrada("x", .texto, titulo: "Nuevo", contenido: "dos")])

        XCTAssertEqual(indice.instantanea().count, 1)
        XCTAssertEqual(indice.instantanea().first?.titulo, "Nuevo")
    }

    func testEliminarPrefijoSoloQuitaEseProveedor() {
        let indice = IndiceBusquedaUniversalIPad()
        indice.reemplazar([
            entrada("problem-bank:1", .problema, titulo: "A", contenido: "A"),
            entrada("problem-error:1", .error, titulo: "B", contenido: "B"),
            entrada("formula:1", .formula, titulo: "C", contenido: "C")
        ])

        indice.eliminar(prefijo: "problem-")

        XCTAssertEqual(indice.instantanea().map(\.claveEstable), ["formula:1"])
    }

    func testLimiteSeRespetaDespuesDelRanking() {
        let indice = IndiceBusquedaUniversalIPad()
        indice.reemplazar((0..<20).map {
            entrada("e\($0)", .texto, titulo: "tema \($0)", contenido: "resistencia")
        })
        var consulta = ConsultaBusquedaUniversalIPad(texto: "resistencia")
        consulta.limite = 4

        XCTAssertEqual(indice.buscar(consulta).count, 4)
    }

    func testAliasTecnicoEncuentraTerminoEquivalente() {
        let repositorio = RepositorioBusquedaTecnicaIPad.compartido
        let original = repositorio.instantanea()
        defer { repositorio.guardar(original) }
        var configuracion = original
        configuracion.alias["resistencia"] = ["resistor"]
        repositorio.guardar(configuracion)

        let indice = IndiceBusquedaUniversalIPad()
        indice.reemplazar([
            entrada("r", .texto, titulo: "Resistencia equivalente", contenido: "Divisor")
        ])

        XCTAssertEqual(indice.buscar(.init(texto: "resistor")).first?.entrada.claveEstable, "r")
    }

    func testToleranciaManuscritaEncuentraErrorOCRCercano() {
        let repositorio = RepositorioBusquedaTecnicaIPad.compartido
        let original = repositorio.instantanea()
        defer { repositorio.guardar(original) }
        var configuracion = original
        configuracion.toleranciaManuscrita = true
        repositorio.guardar(configuracion)

        let indice = IndiceBusquedaUniversalIPad()
        indice.reemplazar([
            entrada("k", .ocr, titulo: "Apunte", contenido: "Kirchhof")
        ])

        XCTAssertFalse(indice.buscar(.init(texto: "Kirchhoff")).isEmpty)
    }

    func testReindexadorCoalescenteAgrupaSolicitudesConcurrentes() {
        let inicioTrabajo = expectation(description: "comenzó reindexado")
        let callbacks = expectation(description: "todos reciben finalización")
        callbacks.expectedFulfillmentCount = 3
        let liberar = DispatchSemaphore(value: 0)
        let bloqueo = NSLock()
        var ejecuciones = 0

        let coordinador = CoordinadorReindexacionBusquedaUniversalIPad(
            vigencia: 60,
            cola: DispatchQueue(label: "test.busqueda.coalescer"),
            trabajo: {
                bloqueo.lock(); ejecuciones += 1; bloqueo.unlock()
                inicioTrabajo.fulfill()
                _ = liberar.wait(timeout: .now() + 2)
            })

        coordinador.solicitar { callbacks.fulfill() }
        coordinador.solicitar { callbacks.fulfill() }
        coordinador.solicitar { callbacks.fulfill() }

        wait(for: [inicioTrabajo], timeout: 1)
        XCTAssertTrue(coordinador.estaIndexando)
        liberar.signal()
        wait(for: [callbacks], timeout: 2)

        bloqueo.lock(); let total = ejecuciones; bloqueo.unlock()
        XCTAssertEqual(total, 1)
        XCTAssertFalse(coordinador.estaIndexando)
    }

    func testReindexadorReutilizaIndiceRecienteYForzarLoRenueva() {
        let bloqueo = NSLock()
        var ejecuciones = 0
        let coordinador = CoordinadorReindexacionBusquedaUniversalIPad(
            vigencia: 300,
            cola: DispatchQueue(label: "test.busqueda.vigencia"),
            trabajo: {
                bloqueo.lock(); ejecuciones += 1; bloqueo.unlock()
            })

        let primera = expectation(description: "primera")
        coordinador.solicitar { primera.fulfill() }
        wait(for: [primera], timeout: 1)

        let reutilizada = expectation(description: "reutilizada")
        coordinador.solicitar { reutilizada.fulfill() }
        wait(for: [reutilizada], timeout: 1)

        bloqueo.lock(); let trasReutilizar = ejecuciones; bloqueo.unlock()
        XCTAssertEqual(trasReutilizar, 1)

        let forzada = expectation(description: "forzada")
        coordinador.solicitar(forzar: true) { forzada.fulfill() }
        wait(for: [forzada], timeout: 1)

        bloqueo.lock(); let final = ejecuciones; bloqueo.unlock()
        XCTAssertEqual(final, 2)
    }

    private func entrada(
        _ clave: String,
        _ categoria: CategoriaBusquedaUniversalIPad,
        titulo: String,
        contenido: String,
        cursoId: UUID? = nil
    ) -> EntradaBusquedaUniversalIPad {
        EntradaBusquedaUniversalIPad(
            claveEstable: clave,
            categoria: categoria,
            titulo: titulo,
            contenido: contenido,
            cursoId: cursoId,
            fecha: Date(timeIntervalSince1970: 1))
    }
}
