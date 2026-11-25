import XCTest
@testable import CuadernUEstudio

final class DuplicacionAgregadoIPadTests: XCTestCase {
    func testNotasAdhesivasDuplicadasUsanIdsNuevosYConexionesInternas() throws {
        let repo = RepositorioNotasAdhesivasIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])

        var izquierda = NotaAdhesivaLienzo()
        var derecha = NotaAdhesivaLienzo()
        izquierda.texto = "Ley de Ohm"
        derecha.texto = "V = I·R"
        izquierda.conexiones = [derecha.id]
        derecha.conexiones = [izquierda.id]
        repo.agregar(izquierda, documento: origen)
        repo.agregar(derecha, documento: origen)
        repo.conectar(origen: izquierda.id, destino: derecha.id,
                      documento: origen)

        let resumen = try repo.duplicar(desde: origen, hacia: destino)
        let notasFuente = repo.notas(origen)
        let notasCopia = repo.notas(destino)
        let conexionesCopia = repo.conexiones(destino)

        XCTAssertEqual(resumen.notas, 2)
        XCTAssertEqual(resumen.conexiones, 1)
        XCTAssertEqual(notasCopia.count, notasFuente.count)
        XCTAssertEqual(conexionesCopia.count, 1)

        let idsFuente = Set(notasFuente.map(\.id))
        let idsCopia = Set(notasCopia.map(\.id))
        XCTAssertTrue(idsFuente.isDisjoint(with: idsCopia))

        for nota in notasCopia {
            XCTAssertTrue(nota.conexiones.allSatisfy { idsCopia.contains($0) })
        }
        for conexion in conexionesCopia {
            XCTAssertTrue(idsCopia.contains(conexion.origenId))
            XCTAssertTrue(idsCopia.contains(conexion.destinoId))
            XCTAssertFalse(idsFuente.contains(conexion.origenId))
            XCTAssertFalse(idsFuente.contains(conexion.destinoId))
        }
    }

    func testNotasAdhesivasNoSobrescribenDestinoExistente() throws {
        let repo = RepositorioNotasAdhesivasIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        var fuente = NotaAdhesivaLienzo()
        fuente.texto = "Fuente"
        var existente = NotaAdhesivaLienzo()
        existente.texto = "No tocar"
        repo.agregar(fuente, documento: origen)
        repo.agregar(existente, documento: destino)

        XCTAssertThrowsError(try repo.duplicar(desde: origen, hacia: destino))
        XCTAssertEqual(repo.notas(destino).map(\.texto), ["No tocar"])
    }

    func testSemanticaRegeneraIdsYRemapeaReferenciasInternas() throws {
        let repo = RepositorioSemanticaIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        let curso = UUID()

        let marcaFuente = try XCTUnwrap(repo.marcar(
            documento: origen,
            claves: ["trazo-r1"],
            tipo: .formula,
            titulo: "Ley de Ohm",
            region: CGRect(x: 20, y: 40, width: 180, height: 90),
            cursoId: curso))

        let interna = ReferenciaFuente(
            tipo: .apuntePropio,
            titulo: "Ejemplo resuelto",
            cursoId: curso,
            documentoId: origen,
            elementoId: "texto-ejemplo")
        let externa = ReferenciaFuente(
            tipo: .libro,
            titulo: "Dorf",
            cursoId: curso,
            hashArchivo: "sha256-libro",
            pagina: 42)
        let enlaceInterno = try XCTUnwrap(repo.vincular(
            documento: origen,
            claves: ["trazo-r1"],
            titulo: "Ver ejemplo",
            destino: interna))
        let enlaceExterno = try XCTUnwrap(repo.vincular(
            documento: origen,
            claves: ["trazo-r1", "texto-ejemplo"],
            titulo: "Consultar libro",
            destino: externa))

        let resumen = try repo.duplicar(desde: origen, hacia: destino)
        XCTAssertEqual(resumen.marcaciones, 1)
        XCTAssertEqual(resumen.referencias, 2)

        let marcasCopia = repo.marcaciones(documento: destino)
        XCTAssertEqual(marcasCopia.count, 1)
        XCTAssertNotEqual(marcasCopia[0].id, marcaFuente.id)
        XCTAssertEqual(marcasCopia[0].documentoId, destino)
        XCTAssertEqual(marcasCopia[0].clavesElemento, marcaFuente.clavesElemento)

        let refsCopia = repo.referencias(documento: destino)
        let copiaInterna = try XCTUnwrap(refsCopia.first {
            $0.tituloOrigen == enlaceInterno.tituloOrigen
        })
        XCTAssertNotEqual(copiaInterna.id, enlaceInterno.id)
        XCTAssertEqual(copiaInterna.documentoOrigen, destino)
        XCTAssertEqual(copiaInterna.destino.documentoId, destino)
        XCTAssertEqual(copiaInterna.destino.elementoId,
                       enlaceInterno.destino.elementoId)
        XCTAssertNotEqual(copiaInterna.destino.id, enlaceInterno.destino.id)

        let copiaExterna = try XCTUnwrap(refsCopia.first {
            $0.tituloOrigen == enlaceExterno.tituloOrigen
        })
        XCTAssertNotEqual(copiaExterna.id, enlaceExterno.id)
        XCTAssertEqual(copiaExterna.documentoOrigen, destino)
        XCTAssertEqual(copiaExterna.destino, enlaceExterno.destino)
    }

    func testSemanticaNoSobrescribeDestinoExistente() throws {
        let repo = RepositorioSemanticaIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        _ = repo.marcar(documento: origen, claves: ["a"], tipo: .importante,
                        titulo: "Fuente", region: nil, cursoId: nil)
        _ = repo.marcar(documento: destino, claves: ["b"], tipo: .duda,
                        titulo: "Destino", region: nil, cursoId: nil)

        XCTAssertThrowsError(try repo.duplicar(desde: origen, hacia: destino))
        XCTAssertEqual(repo.marcaciones(documento: destino).map(\.titulo),
                       ["Destino"])
    }

    func testNavegacionRegeneraIdsYRemapeaFrameDeEscena() throws {
        let repo = RepositorioNavegacionEspacialIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])

        let frame = FrameApunteIPad(
            titulo: "Circuito equivalente",
            region: CGRect(x: 100, y: 240, width: 1240, height: 1754),
            tipo: .a4)
        let escena = EscenaApunteIPad(
            titulo: "Zoom RLC",
            vista: EstadoVistaLienzo(
                zoom: 1.75,
                desplazamiento: CGPoint(x: 320, y: 880)),
            frameId: frame.id)
        let favoritoVigente = FavoritoEspacialTemporalIPad(
            titulo: "Repasar",
            region: CGRect(x: 300, y: 500, width: 400, height: 300),
            vence: Date().addingTimeInterval(86_400))
        let favoritoVencido = FavoritoEspacialTemporalIPad(
            titulo: "Viejo",
            region: CGRect(x: 0, y: 0, width: 10, height: 10),
            vence: Date().addingTimeInterval(-86_400))

        repo.agregar(frame, documento: origen)
        repo.agregar(escena, documento: origen)
        repo.agregar(favoritoVigente, documento: origen)
        repo.agregar(favoritoVencido, documento: origen)

        let resumen = try repo.duplicar(desde: origen, hacia: destino)
        XCTAssertEqual(resumen.frames, 1)
        XCTAssertEqual(resumen.escenas, 1)
        XCTAssertEqual(resumen.favoritos, 1)

        let copia = repo.instantanea(destino)
        XCTAssertEqual(copia.frames.count, 1)
        XCTAssertEqual(copia.escenas.count, 1)
        XCTAssertEqual(copia.favoritos.map(\.titulo), ["Repasar"])
        XCTAssertNotEqual(copia.frames[0].id, frame.id)
        XCTAssertNotEqual(copia.escenas[0].id, escena.id)
        XCTAssertEqual(copia.escenas[0].frameId, copia.frames[0].id)
        XCTAssertEqual(copia.escenas[0].vista, escena.vista)
    }

    func testNavegacionNoSobrescribeDestinoExistente() throws {
        let repo = RepositorioNavegacionEspacialIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        repo.agregar(FrameApunteIPad(
            titulo: "Origen",
            region: CGRect(x: 0, y: 0, width: 100, height: 100),
            tipo: .visible), documento: origen)
        repo.agregar(FrameApunteIPad(
            titulo: "Destino",
            region: CGRect(x: 0, y: 0, width: 200, height: 200),
            tipo: .visible), documento: destino)

        XCTAssertThrowsError(try repo.duplicar(desde: origen, hacia: destino))
        XCTAssertEqual(repo.instantanea(destino).frames.map(\.titulo), ["Destino"])
    }

    func testCoordinadorRechazaMismaIdentidad() {
        let id = idDocumentoAleatorio()
        XCTAssertThrowsError(
            try CoordinadorDuplicacionAgregadoIPad.compartido
                .duplicar(desde: id, hacia: id))
    }

    private func idDocumentoAleatorio(excluyendo: Set<UInt64> = []) -> UInt64 {
        var valor: UInt64
        repeat { valor = UInt64.random(in: 1...UInt64.max) }
        while excluyendo.contains(valor)
        return valor
    }
}
