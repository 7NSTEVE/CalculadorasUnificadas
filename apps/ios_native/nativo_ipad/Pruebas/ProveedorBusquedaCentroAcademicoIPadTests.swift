import XCTest
@testable import CuadernUEstudio

final class ProveedorBusquedaCentroAcademicoIPadTests: XCTestCase {
    func testIndexaNodoConFuenteYNavegacion() throws {
        let curso = UUID()
        let fuente = ReferenciaFuente(
            tipo: .libro, titulo: "Dorf", cursoId: curso,
            documentoId: 77, pagina: 12, segundoAudio: 45)
        var base = BaseAcademica()
        base.conocimiento = [NodoConocimiento(
            titulo: "Thévenin", texto: "Equivalente de dos terminales",
            fuente: fuente, etiquetas: ["circuitos"])]

        let entradas = ProveedorBusquedaCentroAcademicoIPad.construir(desde: base)
        let entrada = try XCTUnwrap(entradas.first)

        XCTAssertEqual(entrada.categoria, .pdf)
        XCTAssertEqual(entrada.cursoId, curso)
        XCTAssertEqual(entrada.fuente, "Dorf")
        XCTAssertEqual(entrada.localizacion.documentoId, 77)
        XCTAssertEqual(entrada.localizacion.pagina, 12)
        XCTAssertEqual(entrada.localizacion.segundoAudio, 45)
    }

    func testNoIndexaFlashcardSuspendida() {
        var base = BaseAcademica()
        var activa = FlashcardAcademica(pregunta: "KCL", respuesta: "Suma de corrientes")
        var suspendida = FlashcardAcademica(pregunta: "KVL", respuesta: "Suma de voltajes")
        suspendida.suspendida = true
        base.flashcards = [activa, suspendida]
        activa.suspendida = false

        let claves = Set(ProveedorBusquedaCentroAcademicoIPad.construir(desde: base)
            .map(\.claveEstable))

        XCTAssertTrue(claves.contains("academico:flashcard:\(activa.id.uuidString)"))
        XCTAssertFalse(claves.contains("academico:flashcard:\(suspendida.id.uuidString)"))
    }

    func testErrorIncluyeRespuestaCorreccionYCausa() throws {
        var base = BaseAcademica()
        let error = ErrorAprendizaje(
            enunciado: "Calcular potencia",
            respuestaDada: "-3 W",
            respuestaCorrecta: "3 W",
            causa: "Convención de signos")
        base.errores = [error]

        let entrada = try XCTUnwrap(ProveedorBusquedaCentroAcademicoIPad
            .construir(desde: base).first { $0.categoria == .error })

        XCTAssertTrue(entrada.contenido.contains("-3 W"))
        XCTAssertTrue(entrada.contenido.contains("3 W"))
        XCTAssertTrue(entrada.contenido.contains("Convención de signos"))
    }

    func testTareaIncluyePasosSinPerderDetalle() throws {
        var base = BaseAcademica()
        var tarea = TareaAcademica(titulo: "Resolver guía", detalle: "Capítulo 2")
        tarea.pasos = [PasoTarea(titulo: "Problema 1"), PasoTarea(titulo: "Problema 2")]
        base.tareas = [tarea]

        let entrada = try XCTUnwrap(ProveedorBusquedaCentroAcademicoIPad
            .construir(desde: base).first { $0.claveEstable.contains("tarea:") })

        XCTAssertTrue(entrada.contenido.contains("Capítulo 2"))
        XCTAssertTrue(entrada.contenido.contains("Problema 1"))
        XCTAssertTrue(entrada.contenido.contains("Problema 2"))
    }

    func testMapeaTranscripcionYPizarraACategoriasCorrectas() {
        let transcripcion = ReferenciaFuente(tipo: .transcripcion, titulo: "Clase")
        let pizarra = ReferenciaFuente(tipo: .pizarra, titulo: "Pizarra")
        var base = BaseAcademica()
        base.conocimiento = [
            NodoConocimiento(titulo: "Audio", texto: "fasores", fuente: transcripcion),
            NodoConocimiento(titulo: "Trazo", texto: "R1 + R2", fuente: pizarra)
        ]

        let entradas = ProveedorBusquedaCentroAcademicoIPad.construir(desde: base)

        XCTAssertTrue(entradas.contains { $0.titulo == "Audio" && $0.categoria == .transcripcion })
        XCTAssertTrue(entradas.contains { $0.titulo == "Trazo" && $0.categoria == .manuscrito })
    }
}
