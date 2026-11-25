import XCTest

final class PlanRecoveryAudioIPadTests: XCTestCase {
    private func segmento(
        _ indice: Int,
        inicio: Double,
        duracion: Double,
        archivo: String? = nil
    ) -> SegmentoAudioResilienteIPad {
        SegmentoAudioResilienteIPad(
            indice: indice,
            archivo: archivo ?? String(format: "segmento-%06d.m4a", indice),
            sha256: String(repeating: String(format: "%x", indice % 16), count: 64),
            bytes: 100,
            inicioSegundos: inicio,
            duracionSegundos: duracion,
            codec: "AAC")
    }

    private func diagnostico(
        segmentos: [SegmentoAudioResilienteIPad],
        faltantes: [String] = [],
        alterados: [String] = [],
        huerfanos: [String] = []
    ) -> DiagnosticoSesionAudioResilienteIPad {
        var manifiesto = ManifiestoAudioResilienteIPad(documentoId: 9)
        manifiesto.segmentos = segmentos
        return DiagnosticoSesionAudioResilienteIPad(
            manifiesto: manifiesto,
            segmentosFaltantes: faltantes,
            segmentosAlterados: alterados,
            archivosHuerfanos: huerfanos)
    }

    func testSesionContinuaIntegraNoInventaHuecos() {
        let a = segmento(0, inicio: 0, duracion: 10)
        let b = segmento(1, inicio: 10, duracion: 12)
        let plan = ConstructorPlanRecoveryAudioIPad.construir(
            diagnostico(segmentos: [b, a]))

        XCTAssertEqual(plan.estado, .integro)
        XCTAssertEqual(plan.intervalosConfiables.map(\.segmentoId), [a.id, b.id])
        XCTAssertTrue(plan.huecos.isEmpty)
        XCTAssertEqual(plan.duracionConfiable, 22, accuracy: 0.001)
        XCTAssertEqual(plan.duracionTimeline, 22, accuracy: 0.001)
        XCTAssertTrue(plan.puedeReproducirParcialmente)
        XCTAssertFalse(plan.requiereDecisionUsuario)
    }

    func testHuecoEntreChunksQuedaExplicito() {
        let a = segmento(0, inicio: 0, duracion: 5)
        let b = segmento(1, inicio: 8, duracion: 4)
        let plan = ConstructorPlanRecoveryAudioIPad.construir(
            diagnostico(segmentos: [a, b]))

        XCTAssertEqual(plan.estado, .parcialRecuperable)
        XCTAssertEqual(plan.huecos.count, 1)
        XCTAssertEqual(plan.huecos[0].inicio, 5, accuracy: 0.001)
        XCTAssertEqual(plan.huecos[0].fin, 8, accuracy: 0.001)
        XCTAssertEqual(plan.duracionConfiable, 9, accuracy: 0.001)
        XCTAssertEqual(plan.duracionTimeline, 12, accuracy: 0.001)
    }

    func testSegmentoFaltanteNoSeTrataComoConfiable() {
        let a = segmento(0, inicio: 0, duracion: 5)
        let b = segmento(1, inicio: 5, duracion: 5)
        let plan = ConstructorPlanRecoveryAudioIPad.construir(
            diagnostico(
                segmentos: [a, b],
                faltantes: [b.archivo]))

        XCTAssertEqual(plan.intervalosConfiables.map(\.archivo), [a.archivo])
        XCTAssertEqual(plan.archivosFaltantes, [b.archivo])
        XCTAssertTrue(plan.huecos.contains {
            abs($0.inicio - 5) < 0.001 && abs($0.fin - 10) < 0.001
        })
        XCTAssertTrue(plan.puedeReproducirParcialmente)
        XCTAssertFalse(plan.requiereDecisionUsuario,
                       "Un faltante conocido se reporta, pero no autoriza a adoptar bytes ajenos")
    }

    func testAlteradoUOrphanExigeDecisionUsuario() {
        let a = segmento(0, inicio: 0, duracion: 5)
        let alterado = ConstructorPlanRecoveryAudioIPad.construir(
            diagnostico(segmentos: [a], alterados: [a.archivo]))
        XCTAssertEqual(alterado.estado, .requiereEvidenciaManual)
        XCTAssertTrue(alterado.requiereDecisionUsuario)
        XCTAssertFalse(alterado.puedeReproducirParcialmente)

        let huerfano = ConstructorPlanRecoveryAudioIPad.construir(
            diagnostico(segmentos: [a], huerfanos: ["segmento-000999.m4a"]))
        XCTAssertEqual(huerfano.estado, .requiereEvidenciaManual)
        XCTAssertTrue(huerfano.requiereDecisionUsuario)
        XCTAssertTrue(huerfano.puedeReproducirParcialmente)
    }

    func testSinSegmentosConfirmadosEsEstadoExplicito() {
        let plan = ConstructorPlanRecoveryAudioIPad.construir(
            diagnostico(segmentos: []))
        XCTAssertEqual(plan.estado, .sinAudioConfirmado)
        XCTAssertFalse(plan.puedeReproducirParcialmente)
        XCTAssertFalse(plan.requiereDecisionUsuario)
    }

    func testHuecosSolapadosSeFusionanSinOcultarMotivos() {
        let a = segmento(0, inicio: 0, duracion: 8)
        let b = segmento(1, inicio: 6, duracion: 7)
        let plan = ConstructorPlanRecoveryAudioIPad.construir(
            diagnostico(
                segmentos: [a, b],
                faltantes: [a.archivo],
                alterados: [b.archivo]))

        XCTAssertEqual(plan.huecos.count, 1)
        XCTAssertEqual(plan.huecos[0].inicio, 0, accuracy: 0.001)
        XCTAssertEqual(plan.huecos[0].fin, 13, accuracy: 0.001)
        XCTAssertTrue(plan.huecos[0].motivo.contains("faltantes")
                      || plan.huecos[0].motivo.contains("confiables"))
    }
}
