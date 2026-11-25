import UIKit
import XCTest
@testable import CuadernUEstudio

final class PreparadorCopiaDocumentoIPadTests: XCTestCase {
    private struct FalloPublicacion: Error {}

    func testPublicacionFallidaRetiraAgregadoYPermiteReintentarMismoUUID() throws {
        let origenId = DocumentoPersistenteID(UUID())
        let destinoId = DocumentoPersistenteID(UUID())
        let origen = try XCTUnwrap(Sesion(idPersistente: origenId))
        let origenClave = origen.documentoPersistenteId
        let destinoClave = destinoId.claveCompatibilidadUInt64
        defer {
            limpiar(origenClave)
            limpiar(destinoClave)
        }

        var nota = NotaAdhesivaLienzo()
        nota.texto = "Debe sobrevivir solo si se publica"
        RepositorioNotasAdhesivasIPad.compartido.agregar(
            nota, documento: origenClave)

        let region = RegionFondoDocumento(
            nombre: "Cuadrícula de circuitos",
            marco: CGRect(x: 20, y: 30, width: 720, height: 520),
            tipo: .cuadricula,
            colorARGB: 0xFFF8_F8F6,
            separacion: 20)
        RepositorioRegionesFondo.compartido.guardar([region], documento: origenClave)

        var edicion = AjustesEdicionImagen()
        edicion.monocromo = true
        edicion.contraste = 1.7
        XCTAssertTrue(RepositorioEdicionesImagenDocumentoIPad.compartido.guardar(
            edicion, documento: origenClave, id: "imagen-prueba"))

        let bytesOrigen = try XCTUnwrap(origen.guardar())
        XCTAssertThrowsError(
            try PreparadorCopiaDocumentoIPad.publicar(
                bytesOrigen,
                destino: destinoId,
                escrituraCanonica: { _ in
                    // La publicación ocurre después de construir el agregado.
                    XCTAssertEqual(RepositorioNotasAdhesivasIPad.compartido
                        .notas(destinoClave).map(\.texto),
                                   ["Debe sobrevivir solo si se publica"])
                    XCTAssertEqual(RepositorioRegionesFondo.compartido
                        .regiones(destinoClave).map(\.nombre),
                                   ["Cuadrícula de circuitos"])
                    XCTAssertEqual(RepositorioEdicionesImagenDocumentoIPad.compartido
                        .instantanea(documento: destinoClave)["imagen-prueba"],
                                   edicion)
                    throw FalloPublicacion()
                }))

        XCTAssertTrue(RepositorioNotasAdhesivasIPad.compartido
            .notas(destinoClave).isEmpty)
        XCTAssertTrue(RepositorioRegionesFondo.compartido
            .regiones(destinoClave).isEmpty)
        XCTAssertTrue(RepositorioEdicionesImagenDocumentoIPad.compartido
            .instantanea(documento: destinoClave).isEmpty)

        var publicado: Data?
        let bytesDestino = try PreparadorCopiaDocumentoIPad.publicar(
            bytesOrigen,
            destino: destinoId,
            escrituraCanonica: { publicado = $0 })

        XCTAssertEqual(publicado, bytesDestino)
        XCTAssertEqual(IdentidadCopiaDocumentoIPad.origen(de: bytesDestino), destinoId)
        XCTAssertEqual(RepositorioNotasAdhesivasIPad.compartido
            .notas(destinoClave).map(\.texto),
                       ["Debe sobrevivir solo si se publica"])
        XCTAssertEqual(RepositorioRegionesFondo.compartido
            .regiones(destinoClave).map(\.nombre),
                       ["Cuadrícula de circuitos"])
    }

    func testCargaCorruptaNoInvocaEscrituraCanonica() throws {
        let destinoId = DocumentoPersistenteID(UUID())
        var invocada = false
        XCTAssertThrowsError(
            try PreparadorCopiaDocumentoIPad.publicar(
                Data([0x54, 0x49, 0x4E, 0x54, 0x41, 0x00, 0xFF]),
                destino: destinoId,
                escrituraCanonica: { _ in invocada = true }))
        XCTAssertFalse(invocada)
    }

    private func limpiar(_ documento: UInt64) {
        _ = CoordinadorDuplicacionAgregadoIPad.compartido
            .descartarCopia(documento: documento)
    }
}
