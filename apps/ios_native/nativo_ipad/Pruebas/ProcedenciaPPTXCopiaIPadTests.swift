import XCTest
@testable import CuadernUEstudio

final class ProcedenciaPPTXCopiaIPadTests: XCTestCase {
    func testDuplicaProcedenciaYPermiteCopiarLaCopia() throws {
        let repo = RepositorioProcedenciaPPTXCopiaIPad.compartido
        let origen = idDocumentoAleatorio()
        let copia = idDocumentoAleatorio(excluyendo: [origen])
        let segundaCopia = idDocumentoAleatorio(excluyendo: [origen, copia])
        defer {
            try? repo.eliminarDocumento(origen)
            try? repo.eliminarDocumento(copia)
            try? repo.eliminarDocumento(segundaCopia)
        }

        let diapositiva = UUID()
        let elemento = UUID()
        let objetoId = UUID().uuidString
        let vinculo = HipervinculoPPTXIPad(
            texto: "Datasheet",
            destino: "https://example.invalid/datasheet",
            externo: true)
        let metadato = MetadatoObjetoPPTXIPad(
            diapositivaId: diapositiva,
            presentacion: "Circuitos II",
            numeroDiapositiva: 12,
            idElementoPPTX: elemento,
            idOffice: "42",
            tipo: .formaVectorial,
            hashFuente: "hash-pptx-inmutable",
            hipervinculo: vinculo,
            pasoAnimacion: 3)
        try repo.registrar(metadato, objetoId: objetoId, documento: origen)

        XCTAssertEqual(try repo.duplicar(desde: origen, hacia: copia), 1)
        XCTAssertEqual(repo.metadato(objetoId: objetoId, documento: copia),
                       metadato)
        XCTAssertEqual(repo.instantaneaCopiada(documento: copia)[objetoId],
                       metadato)

        // El segundo salto no depende de que el origen siga en el repositorio
        // legacy: la fotografía transaccional de la primera copia es suficiente.
        XCTAssertEqual(try repo.duplicar(desde: copia, hacia: segundaCopia), 1)
        XCTAssertEqual(repo.metadato(objetoId: objetoId,
                                     documento: segundaCopia), metadato)
        XCTAssertEqual(try repo.instantaneaEfectiva(documento: segundaCopia),
                       [objetoId: metadato])
    }

    func testConservaClavesQueApuntanABloquesYVectoresCopiados() throws {
        let repo = RepositorioProcedenciaPPTXCopiaIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        defer {
            try? repo.eliminarDocumento(origen)
            try? repo.eliminarDocumento(destino)
        }

        let bloqueId = UUID().uuidString
        let vectorId = UUID().uuidString
        let slide = UUID()
        let metadatoBloque = MetadatoObjetoPPTXIPad(
            diapositivaId: slide,
            presentacion: "Electrónica",
            numeroDiapositiva: 4,
            idElementoPPTX: UUID(),
            tipo: .tabla,
            hashFuente: "hash-presentacion")
        let metadatoVector = MetadatoObjetoPPTXIPad(
            diapositivaId: slide,
            presentacion: "Electrónica",
            numeroDiapositiva: 4,
            idElementoPPTX: UUID(),
            tipo: .formaVectorial,
            hashFuente: "hash-presentacion")
        try repo.registrar(metadatoBloque, objetoId: bloqueId, documento: origen)
        try repo.registrar(metadatoVector, objetoId: vectorId, documento: origen)

        XCTAssertEqual(try repo.duplicar(desde: origen, hacia: destino), 2)
        let copia = repo.instantaneaCopiada(documento: destino)
        XCTAssertEqual(Set(copia.keys), Set([bloqueId, vectorId]))
        XCTAssertEqual(copia[bloqueId], metadatoBloque)
        XCTAssertEqual(copia[vectorId], metadatoVector)
    }

    func testNoSobrescribeDestinoConProcedenciaExistente() throws {
        let repo = RepositorioProcedenciaPPTXCopiaIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        defer {
            try? repo.eliminarDocumento(origen)
            try? repo.eliminarDocumento(destino)
        }

        let fuente = MetadatoObjetoPPTXIPad(
            diapositivaId: UUID(), presentacion: "A", numeroDiapositiva: 1,
            idElementoPPTX: UUID(), tipo: .texto, hashFuente: "hash-a")
        let existente = MetadatoObjetoPPTXIPad(
            diapositivaId: UUID(), presentacion: "B", numeroDiapositiva: 2,
            idElementoPPTX: UUID(), tipo: .imagen, hashFuente: "hash-b")
        try repo.registrar(fuente, objetoId: "fuente", documento: origen)
        try repo.registrar(existente, objetoId: "existente", documento: destino)

        XCTAssertThrowsError(try repo.duplicar(desde: origen, hacia: destino))
        XCTAssertEqual(repo.instantaneaCopiada(documento: destino),
                       ["existente": existente])
    }

    private func idDocumentoAleatorio(excluyendo: Set<UInt64> = []) -> UInt64 {
        var valor: UInt64
        repeat { valor = UInt64.random(in: 1...UInt64.max) }
        while excluyendo.contains(valor)
        return valor
    }
}
