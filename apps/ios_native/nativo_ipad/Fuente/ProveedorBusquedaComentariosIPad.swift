import Foundation

enum ProveedorBusquedaComentariosIPad {
    static func construir(_ hilos: [HiloComentarioDocumentoIPad]) -> [EntradaBusquedaUniversalIPad] {
        hilos.filter { !$0.resuelto }.map { hilo in
            let contenido = hilo.mensajes.map { "\($0.autorNombre): \($0.texto)" }
                .joined(separator: "\n")
            let titulo = hilo.mensajes.first?.texto ?? "Comentario"
            return EntradaBusquedaUniversalIPad(
                claveEstable: "comentario:\(hilo.id.uuidString)",
                categoria: .texto,
                titulo: String(titulo.prefix(120)),
                contenido: contenido,
                palabrasClave: ["comentario", "thread", "discusión"],
                fuente: "Comentarios",
                fecha: hilo.actualizado,
                localizacion: .init(
                    documentoId: hilo.anclaje.documentoId.claveCompatibilidadUInt64,
                    pagina: hilo.anclaje.pagina,
                    frameId: hilo.anclaje.frameId,
                    segundoAudio: hilo.anclaje.segundoAudio,
                    contexto: hilo.anclaje.contexto))
        }
    }

    static func reindexar(
        repositorio: RepositorioComentariosDocumentoIPad = .compartido,
        indice: IndiceBusquedaUniversalIPad = .compartido
    ) {
        indice.eliminar(prefijo: "comentario:")
        indice.upsert(construir(repositorio.buscar("", incluirResueltos: false)))
    }
}
