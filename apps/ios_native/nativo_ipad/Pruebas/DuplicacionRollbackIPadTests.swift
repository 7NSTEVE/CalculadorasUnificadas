import XCTest
@testable import CuadernUEstudio

final class DuplicacionRollbackIPadTests: XCTestCase {
    private struct FalloForzado: Error {}

    func testFalloTrasCadaSidecarRevierteYPermiteReintentarMismoDestino() throws {
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        prepararAgregado(origen)
        defer {
            try? RepositorioEdicionesImagenDocumentoIPad.compartido
                .eliminarDocumento(origen)
            try? RepositorioEdicionesImagenDocumentoIPad.compartido
                .eliminarDocumento(destino)
            try? RepositorioClipsPDFDocumentoIPad.compartido
                .eliminarDocumento(origen)
            try? RepositorioClipsPDFDocumentoIPad.compartido
                .eliminarDocumento(destino)
            try? RepositorioAjustesTextoDocumentoIPad.compartido
                .eliminarDocumento(origen)
            try? RepositorioAjustesTextoDocumentoIPad.compartido
                .eliminarDocumento(destino)
            try? DuplicadorMetadatosSeleccionIPad.compartido
                .eliminarDocumento(destino)
            try? DuplicadorRegionesFondoIPad.compartido
                .eliminarDocumento(origen)
            try? DuplicadorRegionesFondoIPad.compartido
                .eliminarDocumento(destino)
            try? DuplicadorObjetosExternosTintaIPad.compartido
                .eliminarBloques(documento: origen)
            try? DuplicadorObjetosExternosTintaIPad.compartido
                .eliminarBloques(documento: destino)
            try? DuplicadorObjetosExternosTintaIPad.compartido
                .eliminarTecnicos(documento: origen)
            try? DuplicadorObjetosExternosTintaIPad.compartido
                .eliminarTecnicos(documento: destino)
            try? DuplicadorEsquemaCircuitoIPad.compartido
                .eliminarDocumento(origen)
            try? DuplicadorEsquemaCircuitoIPad.compartido
                .eliminarDocumento(destino)
            try? RepositorioProcedenciaPPTXCopiaIPad.compartido
                .eliminarDocumento(origen)
            try? RepositorioProcedenciaPPTXCopiaIPad.compartido
                .eliminarDocumento(destino)
        }

        // Esta prueba enumera el contrato transaccional completo. Si en el
        // futuro se añade un nuevo `Paso`, CaseIterable obliga a ejercitarlo
        // automáticamente en vez de seguir llamando “último” a una etapa vieja.
        for pasoFallo in CoordinadorDuplicacionAgregadoIPad.Paso.allCases {
            XCTAssertThrowsError(
                try CoordinadorDuplicacionAgregadoIPad.compartido.duplicar(
                    desde: origen,
                    hacia: destino,
                    despuesDePaso: { paso in
                        if paso == pasoFallo { throw FalloForzado() }
                    }),
                "Debía fallar después de \(pasoFallo)")
            verificarDestinoLimpio(destino)
        }

        // Después de fallar en todas las fronteras, el mismo UUID destino debe
        // seguir siendo reutilizable para una copia completa y coherente.
        let reintento = try CoordinadorDuplicacionAgregadoIPad.compartido
            .duplicar(desde: origen, hacia: destino)

        XCTAssertEqual(reintento.notasAdhesivas, 1)
        XCTAssertEqual(reintento.marcacionesSemanticas, 1)
        XCTAssertEqual(reintento.frames, 1)
        XCTAssertEqual(reintento.cubiertasRecall, 1)
        XCTAssertEqual(reintento.ajustesFondos, 1)
        XCTAssertEqual(reintento.regionesFondo, 1)
        XCTAssertEqual(reintento.edicionesImagen, 1)
        XCTAssertEqual(reintento.clipsPDF, 1)
        XCTAssertEqual(reintento.ajustesTexto, 1)
        XCTAssertEqual(reintento.bloques, 1)
        XCTAssertEqual(reintento.objetosTecnicos, 1)
        XCTAssertEqual(reintento.esquemaCircuito, 1)
        XCTAssertEqual(reintento.objetosPPTX, 2)
        XCTAssertEqual(RepositorioNotasAdhesivasIPad.compartido
            .notas(destino).map(\.texto), ["Rollback"])
        XCTAssertEqual(RepositorioSemanticaIPad.compartido
            .marcaciones(documento: destino).map(\.titulo), ["Rollback semántico"])
        XCTAssertEqual(RepositorioNavegacionEspacialIPad.compartido
            .instantanea(destino).frames.map(\.titulo), ["Rollback frame"])
        XCTAssertTrue(RepositorioRecallIPad.compartido.activo(destino))
        XCTAssertEqual(RepositorioRecallIPad.compartido.cubiertas(destino).count, 1)
        XCTAssertEqual(RepositorioFondosObjetoIPad.compartido
            .ajustes(documento: destino).count, 1)
        XCTAssertEqual(RepositorioRegionesFondo.compartido
            .regiones(destino).map(\.nombre), ["Región rollback"])
        XCTAssertEqual(RepositorioEdicionesImagenDocumentoIPad.compartido
            .instantanea(documento: destino).count, 1)
        XCTAssertEqual(RepositorioClipsPDFDocumentoIPad.compartido
            .instantanea(documento: destino).count, 1)
        XCTAssertEqual(RepositorioAjustesTextoDocumentoIPad.compartido
            .instantanea(documento: destino).count, 1)
        XCTAssertEqual(RepositorioBloquesLienzo.compartido
            .bloques(documento: destino).map(\.titulo), ["Bloque rollback"])
        XCTAssertEqual(RepositorioObjetosTecnicos.compartido
            .objetos(documento: destino).map(\.etiqueta), ["V"])
        XCTAssertEqual(try DuplicadorEsquemaCircuitoIPad.compartido
            .persistido(documento: destino)?.titulo, "Circuito rollback")
        XCTAssertEqual(RepositorioProcedenciaPPTXCopiaIPad.compartido
            .instantaneaCopiada(documento: destino).count, 2)
    }

    private func verificarDestinoLimpio(_ destino: UInt64,
                                        file: StaticString = #filePath,
                                        line: UInt = #line) {
        XCTAssertTrue(RepositorioNotasAdhesivasIPad.compartido
            .notas(destino).isEmpty, file: file, line: line)
        XCTAssertTrue(RepositorioNotasAdhesivasIPad.compartido
            .conexiones(destino).isEmpty, file: file, line: line)
        XCTAssertTrue(RepositorioSemanticaIPad.compartido
            .marcaciones(documento: destino).isEmpty, file: file, line: line)
        XCTAssertTrue(RepositorioSemanticaIPad.compartido
            .referencias(documento: destino).isEmpty, file: file, line: line)

        let navegacion = RepositorioNavegacionEspacialIPad.compartido
            .instantanea(destino)
        XCTAssertTrue(navegacion.frames.isEmpty, file: file, line: line)
        XCTAssertTrue(navegacion.escenas.isEmpty, file: file, line: line)
        XCTAssertTrue(navegacion.favoritos.isEmpty, file: file, line: line)

        XCTAssertFalse(RepositorioRecallIPad.compartido.activo(destino),
                       file: file, line: line)
        XCTAssertFalse(RepositorioRecallIPad.compartido.soloExamen(destino),
                       file: file, line: line)
        XCTAssertTrue(RepositorioRecallIPad.compartido.cubiertas(destino).isEmpty,
                      file: file, line: line)
        XCTAssertTrue(RepositorioFondosObjetoIPad.compartido
            .ajustes(documento: destino).isEmpty, file: file, line: line)
        XCTAssertTrue(RepositorioRegionesFondo.compartido
            .regiones(destino).isEmpty, file: file, line: line)
        XCTAssertTrue(RepositorioEdicionesImagenDocumentoIPad.compartido
            .instantanea(documento: destino).isEmpty, file: file, line: line)
        XCTAssertTrue(RepositorioClipsPDFDocumentoIPad.compartido
            .instantanea(documento: destino).isEmpty, file: file, line: line)
        XCTAssertTrue(RepositorioAjustesTextoDocumentoIPad.compartido
            .instantanea(documento: destino).isEmpty, file: file, line: line)
        XCTAssertTrue((try? DuplicadorMetadatosSeleccionIPad.compartido
            .instantanea(documento: destino).isEmpty) ?? false,
                      file: file, line: line)
        XCTAssertTrue(RepositorioBloquesLienzo.compartido
            .bloques(documento: destino).isEmpty, file: file, line: line)
        XCTAssertTrue(RepositorioObjetosTecnicos.compartido
            .objetos(documento: destino).isEmpty, file: file, line: line)
        XCTAssertNil(try? DuplicadorEsquemaCircuitoIPad.compartido
            .persistido(documento: destino), file: file, line: line)
        XCTAssertTrue(RepositorioProcedenciaPPTXCopiaIPad.compartido
            .instantaneaCopiada(documento: destino).isEmpty,
                      file: file, line: line)
    }

    private func prepararAgregado(_ documento: UInt64) {
        var nota = NotaAdhesivaLienzo()
        nota.texto = "Rollback"
        RepositorioNotasAdhesivasIPad.compartido.agregar(
            nota, documento: documento)

        _ = RepositorioSemanticaIPad.compartido.marcar(
            documento: documento,
            claves: ["elemento-rollback"],
            tipo: .importante,
            titulo: "Rollback semántico",
            region: CGRect(x: 10, y: 20, width: 100, height: 60),
            cursoId: nil)

        RepositorioNavegacionEspacialIPad.compartido.agregar(
            FrameApunteIPad(
                titulo: "Rollback frame",
                region: CGRect(x: 20, y: 40, width: 640, height: 480),
                tipo: .visible),
            documento: documento)

        var capas = EstadoCapasDocumento()
        capas.capas[0].visible = false
        _ = RepositorioCapas.compartido.guardar(capas, documento: documento)

        RepositorioRecallIPad.compartido.activar(true, documento: documento)
        _ = RepositorioRecallIPad.compartido.agregar(
            documento: documento,
            claves: ["elemento-rollback"],
            region: CGRect(x: 10, y: 20, width: 100, height: 60),
            estilo: .papel,
            titulo: "Cubierta rollback")

        var fondo = AjustesFondoObjetoIPad()
        fondo.esFondo = true
        fondo.bloqueado = true
        fondo.opacidad = 0.5
        _ = RepositorioFondosObjetoIPad.compartido.guardar(
            fondo,
            documento: documento,
            elemento: "medio-rollback",
            nombre: "Printout rollback")

        let region = RegionFondoDocumento(
            nombre: "Región rollback",
            marco: CGRect(x: 20, y: 20, width: 700, height: 420),
            tipo: .cuadricula,
            colorARGB: 0xFFF8_F8F6,
            colorPautaARGB: 0x3340_5060,
            separacion: 24)
        RepositorioRegionesFondo.compartido.guardar([region], documento: documento)

        var imagen = AjustesEdicionImagen()
        imagen.invertir = true
        imagen.contraste = 1.6
        _ = RepositorioEdicionesImagenDocumentoIPad.compartido.guardar(
            imagen, documento: documento, id: "medio-rollback")

        let clip = ClipConFuenteIPad(
            elementoId: "clip-rollback",
            hashFuente: "hash-rollback",
            pagina: 2,
            regionNormalizada: CGRect(x: 0.1, y: 0.2, width: 0.6, height: 0.4),
            forma: .poligonal,
            puntosNormalizados: [
                CGPoint(x: 0.1, y: 0.2),
                CGPoint(x: 0.7, y: 0.2),
                CGPoint(x: 0.6, y: 0.6),
            ])
        _ = RepositorioClipsPDFDocumentoIPad.compartido.registrar(
            clip, documento: documento)

        var texto = AjustesCajaTextoIPad()
        texto.markdown = true
        texto.padding = 18
        _ = RepositorioAjustesTextoDocumentoIPad.compartido.guardar(
            texto, documento: documento, id: "texto-rollback")

        let bloque = BloqueLienzo(
            tipo: .ecuacion,
            titulo: "Bloque rollback",
            contenido: "V = I·R",
            marco: CGRect(x: 80, y: 520, width: 360, height: 180),
            fuenteElementoId: "pptx-rollback")
        RepositorioBloquesLienzo.compartido.guardar([bloque], documento: documento)

        let tecnico = ObjetoTecnico(
            tipo: .vector,
            puntos: [CGPoint(x: 60, y: 760), CGPoint(x: 340, y: 760)],
            etiqueta: "V",
            unidad: "V",
            magnitud: 20)
        RepositorioObjetosTecnicos.compartido.guardar([tecnico], documento: documento)

        var esquema = EsquemaEditable()
        esquema.titulo = "Circuito rollback"
        esquema.nodos.append(NodoEsquema(
            numero: 1, posicion: CGPoint(x: 120, y: 120), nombre: "Vin"))
        esquema.componentes = [ComponenteEsquema(
            referencia: "R1", tipo: .resistor,
            nodoA: 1, nodoB: 0,
            posicion: CGPoint(x: 180, y: 200), valor: 1_000)]
        RepositorioEsquemas.compartido.guardar(esquema, documento: documento)

        let diapositiva = UUID()
        try? RepositorioProcedenciaPPTXCopiaIPad.compartido.registrar(
            MetadatoObjetoPPTXIPad(
                diapositivaId: diapositiva,
                presentacion: "Rollback PPTX",
                numeroDiapositiva: 1,
                idElementoPPTX: UUID(),
                tipo: .ecuacion,
                hashFuente: "pptx-hash-rollback"),
            objetoId: bloque.id.uuidString,
            documento: documento)
        try? RepositorioProcedenciaPPTXCopiaIPad.compartido.registrar(
            MetadatoObjetoPPTXIPad(
                diapositivaId: diapositiva,
                presentacion: "Rollback PPTX",
                numeroDiapositiva: 1,
                idElementoPPTX: UUID(),
                tipo: .formaVectorial,
                hashFuente: "pptx-hash-rollback"),
            objetoId: tecnico.id.uuidString,
            documento: documento)
    }

    private func idDocumentoAleatorio(excluyendo: Set<UInt64> = []) -> UInt64 {
        var valor: UInt64
        repeat { valor = UInt64.random(in: 1...UInt64.max) }
        while excluyendo.contains(valor)
        return valor
    }
}
