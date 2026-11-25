import XCTest
@testable import CuadernUEstudio

final class DuplicacionDestinoInicializadoTests: XCTestCase {
    func testNotasPermitenCacheDestinoVacioCreadoPorLaVista() throws {
        let repo = RepositorioNotasAdhesivasIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        var nota = NotaAdhesivaLienzo()
        nota.texto = "Contenido fuente"
        repo.agregar(nota, documento: origen)

        // `CapaNotasAdhesivasIPad` hace esta lectura al asignar documentoId.
        XCTAssertTrue(repo.notas(destino).isEmpty)
        XCTAssertTrue(repo.conexiones(destino).isEmpty)

        XCTAssertNoThrow(try repo.duplicar(desde: origen, hacia: destino))
        XCTAssertEqual(repo.notas(destino).map(\.texto), ["Contenido fuente"])
    }

    func testNavegacionPermiteJsonDestinoVacioCreadoPorLaVista() throws {
        let repo = RepositorioNavegacionEspacialIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        repo.agregar(FrameApunteIPad(
            titulo: "Fuente",
            region: CGRect(x: 10, y: 20, width: 300, height: 200),
            tipo: .visible), documento: origen)

        // El inicializador del panel llama `instantanea`, que puede persistir
        // un archivo vacío antes de que llegue la copia de tinta.
        let vacio = repo.instantanea(destino)
        XCTAssertTrue(vacio.frames.isEmpty)
        XCTAssertTrue(vacio.escenas.isEmpty)
        XCTAssertTrue(vacio.favoritos.isEmpty)

        XCTAssertNoThrow(try repo.duplicar(desde: origen, hacia: destino))
        XCTAssertEqual(repo.instantanea(destino).frames.map(\.titulo), ["Fuente"])
    }

    private func idDocumentoAleatorio(excluyendo: Set<UInt64> = []) -> UInt64 {
        var valor: UInt64
        repeat { valor = UInt64.random(in: 1...UInt64.max) }
        while excluyendo.contains(valor)
        return valor
    }
}
