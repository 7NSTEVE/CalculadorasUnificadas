import Foundation

enum ProveedorBusquedaElementosReutilizablesIPad {
    static func construir(
        desde instantanea: InstantaneaBibliotecaElementosIPad
    ) -> [EntradaBusquedaUniversalIPad] {
        let colecciones = Dictionary(uniqueKeysWithValues: instantanea.colecciones.map {
            ($0.id, $0)
        })
        return instantanea.elementos.compactMap { elemento in
            guard let coleccion = colecciones[elemento.coleccionId],
                  !coleccion.archivada else { return nil }
            let origen = elemento.documentoOrigen.map { "Documento \($0.description)" }
            let contenido = [elemento.detalle, origen]
                .compactMap { $0 }
                .filter { !$0.isEmpty }
                .joined(separator: "\n")
            return EntradaBusquedaUniversalIPad(
                claveEstable: "elemento-reutilizable:\(elemento.id.uuidString)",
                categoria: .objeto,
                titulo: elemento.nombre,
                contenido: contenido,
                palabrasClave: elemento.etiquetas + [elemento.tipo.nombre, coleccion.nombre],
                fuente: "Elementos · \(coleccion.nombre)",
                fecha: elemento.actualizado,
                localizacion: .init(
                    documentoId: elemento.documentoOrigen?.claveCompatibilidadUInt64,
                    pagina: elemento.paginaOrigen,
                    frameId: elemento.frameOrigen,
                    contexto: "Elemento reutilizable · \(elemento.tipo.nombre)"))
        }
    }

    static func reindexar(
        biblioteca: BibliotecaElementosReutilizablesIPad = .compartido,
        indice: IndiceBusquedaUniversalIPad = .compartido
    ) {
        indice.eliminar(prefijo: "elemento-reutilizable:")
        indice.upsert(construir(desde: biblioteca.instantanea()))
    }
}
