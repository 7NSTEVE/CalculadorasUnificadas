import XCTest
@testable import CuadernUEstudio

final class NavegacionEnlacesInternosIPadTests: XCTestCase {
    private final class GestorFalso: GestionBibliotecaCuadernU {
        var cuadernos: [CuadernoBibliotecaIPad]
        init(_ cuadernos: [CuadernoBibliotecaIPad]) { self.cuadernos = cuadernos }
        func listarCuadernos() throws -> [CuadernoBibliotecaIPad] { cuadernos }
        func crearCuaderno(titulo: String) throws -> CuadernoBibliotecaIPad {
            throw CocoaError(.featureUnsupported)
        }
        func renombrarCuaderno(id: String, nombre: String) throws {
            throw CocoaError(.featureUnsupported)
        }
        func actualizarColor(id: String, indiceColor: Int) throws {
            throw CocoaError(.featureUnsupported)
        }
        func eliminarCuaderno(id: String) throws {
            throw CocoaError(.featureUnsupported)
        }
    }

    private func baseTemporal() throws -> URL {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("CuadernU-Navegacion-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        addTeardownBlock { try? FileManager.default.removeItem(at: url) }
        return url
    }

    private func cuaderno(id: String, titulo: String) -> CuadernoBibliotecaIPad {
        .init(id: id, titulo: titulo, fechaCreacion: Date(timeIntervalSince1970: 1),
              fechaModificacion: Date(timeIntervalSince1970: 2), indiceColor: 0)
    }

    private func registro(id: UUID, titulo: String) -> RegistroPestanaIPad {
        .init(id: id, titulo: titulo, colorARGB: 0xFF0A84FF,
              archivo: "principal-\(id.uuidString).tinta")
    }

    func testResolutorEncuentraDocumentoPorUUIDAunqueTitulosSeanIguales() throws {
        let repo = RepositorioPestanasIPad(directorioBase: try baseTemporal())
        let a = cuaderno(id: "A", titulo: "Circuitos")
        let b = cuaderno(id: "B", titulo: "Circuitos")
        let idA = UUID(), idB = UUID()
        try repo.guardar(.init(abiertas: [registro(id: idA, titulo: "Clase")],
                               cerradas: [], activa: idA),
                          panel: .principal, contexto: a.id)
        try repo.guardar(.init(abiertas: [registro(id: idB, titulo: "Clase")],
                               cerradas: [], activa: idB),
                          panel: .principal, contexto: b.id)

        let resolutor = ResolutorUbicacionDocumentoIPad(
            repositorio: repo, gestor: GestorFalso([a, b]))
        let ubicacion = try XCTUnwrap(resolutor.resolver(DocumentoPersistenteID(idB)))

        XCTAssertEqual(ubicacion.cuadernoId, "B")
        XCTAssertEqual(ubicacion.pestanaId, idB)
        XCTAssertTrue(ubicacion.abierta)
    }

    func testResolutorRevierteClaveCompatibilidadDelIndiceAlUUIDCorrecto() throws {
        let repo = RepositorioPestanasIPad(directorioBase: try baseTemporal())
        let a = cuaderno(id: "A", titulo: "Física")
        let b = cuaderno(id: "B", titulo: "Física")
        let idA = UUID(), idB = UUID()
        try repo.guardar(.init(abiertas: [registro(id: idA, titulo: "Tema")],
                               cerradas: [], activa: idA),
                          panel: .principal, contexto: a.id)
        try repo.guardar(.init(abiertas: [registro(id: idB, titulo: "Tema")],
                               cerradas: [], activa: idB),
                          panel: .principal, contexto: b.id)
        let resolutor = ResolutorUbicacionDocumentoIPad(
            repositorio: repo, gestor: GestorFalso([a, b]))
        let clave = DocumentoPersistenteID(idB).claveCompatibilidadUInt64

        let ubicacion = try XCTUnwrap(
            resolutor.resolver(claveCompatibilidad: clave))
        XCTAssertEqual(ubicacion.cuadernoId, "B")
        XCTAssertEqual(ubicacion.pestanaId, idB)
        XCTAssertNotEqual(
            DocumentoPersistenteID(idA).claveCompatibilidadUInt64, clave)
    }

    func testResolutorDistinguePestanaCerradaSinRestaurarlaEnSilencio() throws {
        let repo = RepositorioPestanasIPad(directorioBase: try baseTemporal())
        let cuaderno = cuaderno(id: "uno", titulo: "Física")
        let activa = UUID(), cerrada = UUID()
        try repo.guardar(.init(
            abiertas: [registro(id: activa, titulo: "Actual")],
            cerradas: [registro(id: cerrada, titulo: "Archivada")],
            activa: activa), panel: .principal, contexto: cuaderno.id)

        let resolutor = ResolutorUbicacionDocumentoIPad(
            repositorio: repo, gestor: GestorFalso([cuaderno]))
        let ubicacion = try XCTUnwrap(
            resolutor.resolver(DocumentoPersistenteID(cerrada)))

        XCTAssertEqual(ubicacion.cuadernoId, "uno")
        XCTAssertEqual(ubicacion.pestanaId, cerrada)
        XCTAssertFalse(ubicacion.abierta)
    }

    func testResolutorNoConfundeDocumentoInexistenteConPestanaActiva() throws {
        let repo = RepositorioPestanasIPad(directorioBase: try baseTemporal())
        let cuaderno = cuaderno(id: "uno", titulo: "Electrónica")
        let activa = UUID()
        try repo.guardar(.init(abiertas: [registro(id: activa, titulo: "A")],
                               cerradas: [], activa: activa),
                          panel: .principal, contexto: cuaderno.id)
        let resolutor = ResolutorUbicacionDocumentoIPad(
            repositorio: repo, gestor: GestorFalso([cuaderno]))

        XCTAssertNil(resolutor.resolver(DocumentoPersistenteID(UUID())))
        XCTAssertNil(resolutor.resolver(claveCompatibilidad: 42))
    }
}
