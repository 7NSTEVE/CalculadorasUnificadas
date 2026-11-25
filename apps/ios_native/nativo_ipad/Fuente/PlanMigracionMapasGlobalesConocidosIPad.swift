import Foundation

struct ResultadoMigracionMapasGlobalesIPad: Equatable {
    var documentoId: DocumentoPersistenteID
    var objetosTecnicos: ResultadoMigracionMapaJSONIPad?
    var bloques: ResultadoMigracionMapaJSONIPad?
    var fondosObjeto: ResultadoMigracionPrefijosJSONIPad?
}

enum PlanMigracionMapasGlobalesConocidosIPad {
    /// Rutas confirmadas en implementaciones existentes del repo. Se migran sólo
    /// si el archivo está presente; no crea archivos vacíos ni toca serialización.
    static func ejecutar(
        documentoId: DocumentoPersistenteID,
        transacciones: CoordinadorPersistenciaTransaccionalIPad = .compartido,
        archivos: FileManager = .default
    ) throws -> ResultadoMigracionMapasGlobalesIPad {
        let documentos = archivos.urls(for: .documentDirectory,
                                       in: .userDomainMask)[0]
        let soporte = archivos.urls(for: .applicationSupportDirectory,
                                    in: .userDomainMask)[0]
        let objetos = documentos
            .appendingPathComponent("ObjetosTecnicos", isDirectory: true)
            .appendingPathComponent("objetos.json")
        let bloques = documentos
            .appendingPathComponent("Bloques", isDirectory: true)
            .appendingPathComponent("bloques.json")
        let fondos = soporte
            .appendingPathComponent("Tinta/FondosObjeto", isDirectory: true)
            .appendingPathComponent("fondos-objeto.json")

        let clave = documentoId.claveCompatibilidadUInt64
        let objetosResultado: ResultadoMigracionMapaJSONIPad? =
            archivos.fileExists(atPath: objetos.path)
                ? try MigradorMapaJSONIdentidadIPad.migrar(
                    documentoId: documentoId,
                    claveLegacy: clave,
                    archivo: objetos,
                    transacciones: transacciones,
                    archivos: archivos)
                : nil

        let bloquesResultado: ResultadoMigracionMapaJSONIPad? =
            archivos.fileExists(atPath: bloques.path)
                ? try MigradorMapaJSONIdentidadIPad.migrar(
                    documentoId: documentoId,
                    claveLegacy: clave,
                    archivo: bloques,
                    transacciones: transacciones,
                    archivos: archivos)
                : nil

        let fondosResultado: ResultadoMigracionPrefijosJSONIPad? =
            archivos.fileExists(atPath: fondos.path)
                ? try MigradorPrefijosJSONIdentidadIPad.migrar(
                    documentoId: documentoId,
                    claveLegacy: clave,
                    archivo: fondos,
                    campos: ["valores", "nombres"],
                    transacciones: transacciones,
                    archivos: archivos)
                : nil

        return .init(documentoId: documentoId,
                     objetosTecnicos: objetosResultado,
                     bloques: bloquesResultado,
                     fondosObjeto: fondosResultado)
    }
}
