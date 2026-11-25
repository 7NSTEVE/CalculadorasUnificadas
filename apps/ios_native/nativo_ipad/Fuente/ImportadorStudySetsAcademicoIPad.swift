import Foundation

struct ResumenImportacionStudySetsIPad: Equatable {
    var creadas: Int
    var duplicadas: Int
}

enum ImportadorStudySetsAcademicoIPad {
    static func importar(_ texto: String, cursoId: UUID?,
                         referencia: ReferenciaFuente? = nil,
                         separador: Character? = nil,
                         centro: CentroAcademico = .compartido) throws
        -> ResumenImportacionStudySetsIPad {
        let filas = try ParserStudySetCSVIPad.analizar(texto, separador: separador)
        return centro.transaccion { base in
            var existentes = Set(base.flashcards.map {
                clave($0.pregunta, $0.respuesta, $0.cursoId)
            })
            var creadas = 0, duplicadas = 0
            for fila in filas {
                let k = clave(fila.pregunta, fila.respuesta, cursoId)
                guard existentes.insert(k).inserted else { duplicadas += 1; continue }
                base.flashcards.append(FlashcardAcademica(
                    pregunta: fila.pregunta,
                    respuesta: fila.respuesta,
                    cursoId: cursoId,
                    referencia: referencia))
                creadas += 1
            }
            return .init(creadas: creadas, duplicadas: duplicadas)
        }
    }

    private static func clave(_ pregunta: String, _ respuesta: String,
                              _ cursoId: UUID?) -> String {
        "\(cursoId?.uuidString ?? "sin-curso")|\(Normalizador.texto(pregunta))|\(Normalizador.texto(respuesta))"
    }
}
