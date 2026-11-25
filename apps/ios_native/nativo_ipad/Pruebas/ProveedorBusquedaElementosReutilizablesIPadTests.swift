import XCTest
@testable import CuadernUEstudio

final class ProveedorBusquedaElementosReutilizablesIPadTests: XCTestCase {
    func testElementoTecnicoConservaOrigenYEtiquetas() {
        let coleccion = ColeccionElementosReutilizablesIPad(nombre: "Circuitos")
        let documento = DocumentoPersistenteID(UUID(uuidString: "11111111-2222-3333-4444-555555555555")!)
        let recurso = ReferenciaRecursoElementoIPad(
            hashSHA256: String(repeating: "a", count: 64), extensionArchivo: "cuitem", bytes: 10)
        let elemento = ElementoReutilizableIPad(
            coleccionId: coleccion.id, tipo: .circuito, nombre: "Divisor resistivo",
            detalle: "R1 y R2 en serie", etiquetas: ["resistencia", "voltaje"],
            recurso: recurso, anchoNatural: 200, altoNatural: 100,
            documentoOrigen: documento, paginaOrigen: 3)

        let entradas = ProveedorBusquedaElementosReutilizablesIPad.construir(
            desde: .init(colecciones: [coleccion], elementos: [elemento]))
        XCTAssertEqual(entradas.count, 1)
        XCTAssertEqual(entradas[0].categoria, .objeto)
        XCTAssertEqual(entradas[0].titulo, "Divisor resistivo")
        XCTAssertTrue(entradas[0].palabrasClave.contains("Circuito"))
        XCTAssertTrue(entradas[0].palabrasClave.contains("Circuitos"))
        XCTAssertEqual(entradas[0].localizacion.pagina, 3)
        XCTAssertEqual(entradas[0].localizacion.documentoId, documento.claveCompatibilidadUInt64)
    }

    func testColeccionArchivadaNoSeIndexa() {
        var coleccion = ColeccionElementosReutilizablesIPad(nombre: "Viejos")
        coleccion.archivada = true
        let recurso = ReferenciaRecursoElementoIPad(
            hashSHA256: String(repeating: "b", count: 64), extensionArchivo: "bin", bytes: 1)
        let elemento = ElementoReutilizableIPad(
            coleccionId: coleccion.id, tipo: .figura, nombre: "Flecha",
            recurso: recurso, anchoNatural: 10, altoNatural: 10)

        XCTAssertTrue(ProveedorBusquedaElementosReutilizablesIPad.construir(
            desde: .init(colecciones: [coleccion], elementos: [elemento])).isEmpty)
    }
}
