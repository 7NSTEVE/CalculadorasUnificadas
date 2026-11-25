import Foundation

enum ResultadoSincronizacionDudaRevisionIPad: Equatable {
    case creada
    case yaExistia
    case noEsDuda
    case problemaNoDisponible
}

enum PreparadorDudaRevisionSolucionIPad {
    static func contenido(
        revision: RevisionSolucionPostIntentoIPad,
        problema: ProblemaBancoIPad
    ) -> (pregunta: String, contexto: String) {
        let nota = revision.nota.trimmingCharacters(in: .whitespacesAndNewlines)
        let paso = revision.paso.trimmingCharacters(in: .whitespacesAndNewlines)
        let correccion = revision.correccion.trimmingCharacters(in: .whitespacesAndNewlines)
        let pregunta = nota.isEmpty
            ? "¿Qué me falta entender para justificar correctamente este procedimiento?"
            : nota
        var lineas = [
            ServicioRevisionSolucionPostIntentoIPad.marcador(
                intentoId: revision.intentoId),
            "Problema: \(problema.enunciado)",
        ]
        if !paso.isEmpty { lineas.append("Punto de duda: \(paso)") }
        if !correccion.isEmpty { lineas.append("Corrección observada: \(correccion)") }
        lineas.append("Frame: \(revision.frameId.uuidString)")
        return (pregunta, lineas.joined(separator: "\n"))
    }
}

enum SincronizadorDudaRevisionSolucionIPad {
    @discardableResult
    static func sincronizar(
        _ revision: RevisionSolucionPostIntentoIPad
    ) -> ResultadoSincronizacionDudaRevisionIPad {
        guard revision.resultado == .duda else { return .noEsDuda }
        let centro = CentroEstudioTecnicoIPad.compartido
        let instantanea = centro.instantanea()
        guard let problema = instantanea.problemas.first(where: {
            $0.id == revision.problemaId
        }) else { return .problemaNoDisponible }

        let marca = ServicioRevisionSolucionPostIntentoIPad.marcador(
            intentoId: revision.intentoId)
        if instantanea.dudas.contains(where: { $0.contexto.contains(marca) }) {
            return .yaExistia
        }

        let contenido = PreparadorDudaRevisionSolucionIPad.contenido(
            revision: revision,
            problema: problema)
        _ = centro.agregarDuda(
            contenido.pregunta,
            contexto: contenido.contexto,
            cursoId: problema.cursoId,
            fuente: problema.fuente)
        return .creada
    }
}
