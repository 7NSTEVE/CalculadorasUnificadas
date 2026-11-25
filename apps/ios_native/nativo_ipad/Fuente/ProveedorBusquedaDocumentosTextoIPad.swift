import Foundation

enum ProveedorBusquedaDocumentosTextoIPad {
    static func construir(
        repositorio: RepositorioDocumentosTextoIPad = .compartido
    ) -> [EntradaBusquedaUniversalIPad] {
        repositorio.listar().flatMap { documento in
            documento.bloques.enumerated().compactMap { indice, bloque in
                let texto: String
                switch bloque.tipo {
                case .tabla:
                    texto = bloque.tabla?.filas.flatMap { $0.map(\.texto) }.joined(separator: " ") ?? ""
                case .imagen, .archivo:
                    texto = [bloque.adjunto?.nombre, bloque.adjunto?.descripcionAccesible]
                        .compactMap { $0 }.joined(separator: " ")
                case .divisor:
                    return nil
                default:
                    texto = bloque.texto
                }
                guard !texto.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else { return nil }
                let categoria: CategoriaBusquedaUniversalIPad = bloque.tipo == .ecuacion ? .formula : .texto
                return EntradaBusquedaUniversalIPad(
                    claveEstable: "textdoc:\(documento.id.description):\(bloque.id.uuidString)",
                    categoria: categoria,
                    titulo: documento.titulo,
                    contenido: texto,
                    palabrasClave: documento.etiquetas + [bloque.tipo.rawValue],
                    cursoId: documento.cursoId,
                    fuente: "Text Document",
                    fecha: bloque.modificado,
                    localizacion: .init(
                        documentoId: documento.id.claveCompatibilidadUInt64,
                        contexto: "textdoc:\(documento.id.description):bloque:\(bloque.id.uuidString):indice:\(indice)"))
            }
        }
    }

    static func reindexar(
        repositorio: RepositorioDocumentosTextoIPad = .compartido,
        indice: IndiceBusquedaUniversalIPad = .compartido
    ) {
        indice.eliminar(prefijo: "textdoc:")
        indice.upsert(construir(repositorio: repositorio))
    }
}