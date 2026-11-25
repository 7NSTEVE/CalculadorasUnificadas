import Foundation

/// Adapta la fuente académica existente al índice universal. No copia estado:
/// reconstruye entradas a partir de `BaseAcademica` y conserva localizadores de
/// la `ReferenciaFuente` original para poder navegar después al contexto exacto.
enum ProveedorBusquedaCentroAcademicoIPad {
    static func construir(desde base: BaseAcademica) -> [EntradaBusquedaUniversalIPad] {
        var salida: [EntradaBusquedaUniversalIPad] = []

        for nodo in base.conocimiento {
            salida.append(EntradaBusquedaUniversalIPad(
                claveEstable: "academico:nodo:\(nodo.id.uuidString)",
                categoria: categoria(nodo.fuente.tipo), titulo: nodo.titulo,
                contenido: nodo.texto, palabrasClave: nodo.etiquetas,
                cursoId: nodo.fuente.cursoId, fuente: nodo.fuente.titulo,
                fecha: nodo.fecha, localizacion: localizacion(nodo.fuente)))
        }

        for tarjeta in base.flashcards where !tarjeta.suspendida {
            salida.append(EntradaBusquedaUniversalIPad(
                claveEstable: "academico:flashcard:\(tarjeta.id.uuidString)",
                categoria: .texto, titulo: tarjeta.pregunta, contenido: tarjeta.respuesta,
                palabrasClave: ["flashcard", "repaso"], cursoId: tarjeta.cursoId,
                fuente: tarjeta.referencia?.titulo, fecha: tarjeta.creada,
                localizacion: tarjeta.referencia.map(localizacion) ?? .init(
                    contexto: "Tarjeta de repaso")))
        }

        for error in base.errores where !error.resuelto {
            salida.append(EntradaBusquedaUniversalIPad(
                claveEstable: "academico:error:\(error.id.uuidString)",
                categoria: .error, titulo: error.enunciado,
                contenido: [error.respuestaDada, error.respuestaCorrecta, error.causa]
                    .filter { !$0.isEmpty }.joined(separator: "\n"),
                palabrasClave: ["error", error.causa].filter { !$0.isEmpty },
                cursoId: error.cursoId, fuente: error.referencia?.titulo,
                fecha: error.fecha,
                localizacion: error.referencia.map(localizacion) ?? .init(
                    contexto: error.causa)))
        }

        for tarea in base.tareas where tarea.estado != .archivada {
            let contenidoTarea = ([tarea.detalle] + tarea.pasos.map(\.titulo))
                .filter { !$0.isEmpty }.joined(separator: "\n")
            salida.append(EntradaBusquedaUniversalIPad(
                claveEstable: "academico:tarea:\(tarea.id.uuidString)",
                categoria: .texto, titulo: tarea.titulo, contenido: contenidoTarea,
                palabrasClave: tarea.etiquetas + ["tarea"], cursoId: tarea.cursoId,
                fuente: tarea.referencia?.titulo, fecha: tarea.fechaCreacion,
                localizacion: tarea.referencia.map(localizacion) ?? .init(
                    contexto: "Tarea académica")))
        }

        for captura in base.bandeja where !captura.procesada {
            salida.append(EntradaBusquedaUniversalIPad(
                claveEstable: "academico:captura:\(captura.id.uuidString)",
                categoria: captura.tipo == .enlace ? .fuente : .texto,
                titulo: "Captura", contenido: captura.contenido,
                palabrasClave: captura.etiquetasSugeridas,
                cursoId: captura.cursoConfirmado ?? captura.cursoSugerido,
                fecha: captura.fecha,
                localizacion: .init(contexto: "Bandeja de entrada")))
        }

        for sesion in base.sesiones {
            salida.append(EntradaBusquedaUniversalIPad(
                claveEstable: "academico:sesion:\(sesion.id.uuidString)",
                categoria: .texto, titulo: sesion.titulo,
                contenido: [sesion.objetivo, sesion.resumenPostClase]
                    .filter { !$0.isEmpty }.joined(separator: "\n"),
                palabrasClave: ["clase", "sesion"], cursoId: sesion.cursoId,
                fecha: sesion.inicio,
                localizacion: .init(documentoId: sesion.documentoId,
                                    contexto: sesion.titulo)))
        }

        for evento in base.timeline {
            guard !evento.titulo.isEmpty || !evento.detalle.isEmpty else { continue }
            salida.append(EntradaBusquedaUniversalIPad(
                claveEstable: "academico:evento:\(evento.id.uuidString)",
                categoria: evento.tipo == .audio ? .transcripcion : .texto,
                titulo: evento.titulo, contenido: evento.detalle,
                palabrasClave: [evento.tipo.rawValue], cursoId: evento.cursoId,
                fuente: evento.referencia?.titulo, fecha: evento.fecha,
                localizacion: evento.referencia.map(localizacion) ?? .init(
                    contexto: evento.tipo.rawValue)))
        }

        return salida
    }

    static func reindexar(
        centro: CentroAcademico = .compartido,
        indice: IndiceBusquedaUniversalIPad = .compartido
    ) {
        indice.eliminar(prefijo: "academico:")
        indice.upsert(construir(desde: centro.instantanea()))
    }

    private static func categoria(_ tipo: TipoFuente) -> CategoriaBusquedaUniversalIPad {
        switch tipo {
        case .transcripcion, .audioClase: return .transcripcion
        case .libro, .diapositivaProfesor: return .pdf
        case .web: return .fuente
        case .pizarra, .apuntePropio: return .manuscrito
        default: return .texto
        }
    }

    private static func localizacion(_ referencia: ReferenciaFuente)
        -> LocalizacionBusquedaUniversalIPad {
        .init(documentoId: referencia.documentoId, pagina: referencia.pagina,
              segundoAudio: referencia.segundoAudio,
              contexto: referencia.elementoId ?? referencia.titulo)
    }
}

enum ReindexadorBusquedaUniversalIPad {
    static func reindexarTodo(indice: IndiceBusquedaUniversalIPad = .compartido) {
        ProveedorBusquedaCentroAcademicoIPad.reindexar(indice: indice)
        ProveedorBusquedaEstudioTecnicoIPad.reindexar(indice: indice)
    }
}
