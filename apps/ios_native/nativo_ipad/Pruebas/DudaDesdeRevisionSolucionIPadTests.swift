import XCTest
@testable import CuadernUEstudio

final class DudaDesdeRevisionSolucionIPadTests: XCTestCase {
    func testConstruyePreguntaConContextoExactoDelIntento() {
        let intento = UUID()
        let frame = UUID()
        let problema = ProblemaBancoIPad(
            enunciado: "Calcule la corriente usando KVL")
        let revision = RevisionSolucionPostIntentoIPad(
            documentoId: 55,
            frameId: frame,
            problemaId: problema.id,
            intentoId: intento,
            resultado: .duda,
            paso: "No entiendo por qué V_R es negativa",
            correccion: "La polaridad se define con la convención pasiva",
            nota: "¿Cuándo debo invertir el signo de una caída de tensión?")

        let contenido = PreparadorDudaRevisionSolucionIPad.contenido(
            revision: revision,
            problema: problema)

        XCTAssertEqual(
            contenido.pregunta,
            "¿Cuándo debo invertir el signo de una caída de tensión?")
        XCTAssertTrue(contenido.contexto.contains(
            ServicioRevisionSolucionPostIntentoIPad.marcador(intentoId: intento)))
        XCTAssertTrue(contenido.contexto.contains(problema.enunciado))
        XCTAssertTrue(contenido.contexto.contains(frame.uuidString))
        XCTAssertTrue(contenido.contexto.contains("No entiendo por qué V_R es negativa"))
    }

    func testPreguntaVaciaUsaFallbackUtil() {
        let problema = ProblemaBancoIPad(enunciado: "Determine R")
        let revision = RevisionSolucionPostIntentoIPad(
            documentoId: 1,
            frameId: UUID(),
            problemaId: problema.id,
            intentoId: UUID(),
            resultado: .duda)

        let contenido = PreparadorDudaRevisionSolucionIPad.contenido(
            revision: revision,
            problema: problema)

        XCTAssertFalse(contenido.pregunta.isEmpty)
        XCTAssertTrue(contenido.contexto.contains("Problema: Determine R"))
    }

    func testMarcadoresDeIntentosDistintosNoColisionan() {
        let a = ServicioRevisionSolucionPostIntentoIPad.marcador(intentoId: UUID())
        let b = ServicioRevisionSolucionPostIntentoIPad.marcador(intentoId: UUID())
        XCTAssertNotEqual(a, b)
    }
}
