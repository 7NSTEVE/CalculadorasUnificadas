import UIKit
import XCTest
@testable import CuadernUEstudio

final class DuplicacionObjetosExternosTintaIPadTests: XCTestCase {
    func testDuplicaBloquesConIdentidadesEstablesYVerificacionDeDisco() throws {
        let duplicador = DuplicadorObjetosExternosTintaIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        defer {
            try? duplicador.eliminarBloques(documento: origen)
            try? duplicador.eliminarBloques(documento: destino)
        }

        var tabla = TablaLienzo(filas: 2, columnas: 2)
        tabla.celdas[0].texto = "V"
        tabla.celdas[1].texto = "I"
        tabla.celdas[2].texto = "20"
        tabla.celdas[3].texto = "2"

        let nodoA = NodoDiagrama(tipo: .bloque, etiqueta: "Vin",
                                 marco: CGRect(x: 0, y: 0, width: 80, height: 50))
        let nodoB = NodoDiagrama(tipo: .bloque, etiqueta: "Vout",
                                 marco: CGRect(x: 180, y: 0, width: 80, height: 50))
        let conexion = ConexionDiagrama(origen: nodoA.id, destino: nodoB.id,
                                       etiqueta: "H(s)")
        let diagrama = DiagramaEditable(nodos: [nodoA, nodoB],
                                        conexiones: [conexion])

        let bloqueTabla = BloqueLienzo(
            tipo: .tabla, titulo: "Mediciones", contenido: "",
            marco: CGRect(x: 40, y: 80, width: 420, height: 240),
            tabla: tabla, fuenteElementoId: "pptx-tabla-1")
        let bloqueDiagrama = BloqueLienzo(
            tipo: .diagramaBloques, titulo: "Sistema", contenido: "",
            marco: CGRect(x: 40, y: 360, width: 520, height: 260),
            diagrama: diagrama, fuenteElementoId: "pptx-diagrama-2")

        RepositorioBloquesLienzo.compartido.guardar(
            [bloqueTabla, bloqueDiagrama], documento: origen)
        XCTAssertEqual(try duplicador.duplicarBloques(
            desde: origen, hacia: destino), 2)

        let copiaMemoria = RepositorioBloquesLienzo.compartido
            .bloques(documento: destino)
        let copiaDisco = try duplicador.bloquesPersistidos(documento: destino)
        XCTAssertEqual(copiaMemoria, [bloqueTabla, bloqueDiagrama])
        XCTAssertEqual(copiaDisco, [bloqueTabla, bloqueDiagrama])
        XCTAssertEqual(copiaDisco.map(\.id), [bloqueTabla.id, bloqueDiagrama.id])
        XCTAssertEqual(copiaDisco[1].diagrama?.conexiones.first?.origen, nodoA.id)
        XCTAssertEqual(copiaDisco[1].diagrama?.conexiones.first?.destino, nodoB.id)
    }

    func testDuplicaObjetosTecnicosConIdentidadYEstiloEstables() throws {
        let duplicador = DuplicadorObjetosExternosTintaIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        defer {
            try? duplicador.eliminarTecnicos(documento: origen)
            try? duplicador.eliminarTecnicos(documento: destino)
        }

        var estilo = EstiloTecnico()
        estilo.colorARGB = 0xFF12_3456
        estilo.grosor = 3.5
        estilo.opacidad = 0.72
        estilo.rellenoARGB = 0x4412_3456
        let vector = ObjetoTecnico(
            tipo: .vector,
            puntos: [CGPoint(x: 30, y: 50), CGPoint(x: 270, y: 140)],
            etiqueta: "F",
            unidad: "N",
            magnitud: 12.5,
            estilo: estilo,
            bloqueado: true)
        let circuito = ObjetoTecnico(
            tipo: .plantillaElectronica,
            puntos: [CGPoint(x: 80, y: 250), CGPoint(x: 380, y: 250)],
            etiqueta: "R1",
            estilo: estilo)

        RepositorioObjetosTecnicos.compartido.guardar(
            [vector, circuito], documento: origen)
        XCTAssertEqual(try duplicador.duplicarTecnicos(
            desde: origen, hacia: destino), 2)

        let copiaMemoria = RepositorioObjetosTecnicos.compartido
            .objetos(documento: destino)
        let copiaDisco = try duplicador.tecnicosPersistidos(documento: destino)
        XCTAssertEqual(copiaMemoria, [vector, circuito])
        XCTAssertEqual(copiaDisco, [vector, circuito])
        XCTAssertEqual(copiaDisco.map(\.id), [vector.id, circuito.id])
    }

    func testRechazaDestinosConContenidoSinSobrescribirlos() throws {
        let duplicador = DuplicadorObjetosExternosTintaIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        defer {
            try? duplicador.eliminarBloques(documento: origen)
            try? duplicador.eliminarBloques(documento: destino)
            try? duplicador.eliminarTecnicos(documento: origen)
            try? duplicador.eliminarTecnicos(documento: destino)
        }

        let fuente = BloqueLienzo(tipo: .texto, titulo: "Fuente", contenido: "A",
                                  marco: CGRect(x: 0, y: 0, width: 120, height: 80))
        let existente = BloqueLienzo(tipo: .texto, titulo: "Destino", contenido: "B",
                                     marco: CGRect(x: 0, y: 0, width: 120, height: 80))
        RepositorioBloquesLienzo.compartido.guardar([fuente], documento: origen)
        RepositorioBloquesLienzo.compartido.guardar([existente], documento: destino)
        XCTAssertThrowsError(try duplicador.duplicarBloques(
            desde: origen, hacia: destino))
        XCTAssertEqual(RepositorioBloquesLienzo.compartido
            .bloques(documento: destino), [existente])

        let tecnicoFuente = ObjetoTecnico(
            tipo: .flecha,
            puntos: [CGPoint(x: 0, y: 0), CGPoint(x: 100, y: 100)])
        let tecnicoExistente = ObjetoTecnico(
            tipo: .circulo,
            puntos: [CGPoint(x: 20, y: 20), CGPoint(x: 60, y: 20)])
        RepositorioObjetosTecnicos.compartido.guardar(
            [tecnicoFuente], documento: origen)
        RepositorioObjetosTecnicos.compartido.guardar(
            [tecnicoExistente], documento: destino)
        XCTAssertThrowsError(try duplicador.duplicarTecnicos(
            desde: origen, hacia: destino))
        XCTAssertEqual(RepositorioObjetosTecnicos.compartido
            .objetos(documento: destino), [tecnicoExistente])
    }

    private func idDocumentoAleatorio(excluyendo: Set<UInt64> = []) -> UInt64 {
        var valor: UInt64
        repeat { valor = UInt64.random(in: 1...UInt64.max) }
        while excluyendo.contains(valor)
        return valor
    }
}
