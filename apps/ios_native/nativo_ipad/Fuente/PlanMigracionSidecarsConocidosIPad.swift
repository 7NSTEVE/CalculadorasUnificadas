import Foundation

struct PasoPlanMigracionSidecarIPad: Equatable, Identifiable {
    enum Tipo: String, Equatable {
        case archivoPorDocumento
        case directorioPorDocumento
    }

    var id: String { "\(tipo.rawValue):\(nombre)" }
    var nombre: String
    var tipo: Tipo
    var origen: URL
    var destino: URL
    var existeLegacy: Bool
    var existeDurable: Bool
}

struct ReportePlanMigracionSidecarsIPad: Equatable {
    var documentoId: DocumentoPersistenteID
    var pasos: [PasoPlanMigracionSidecarIPad]
    var legacyEncontrados: Int { pasos.filter(\.existeLegacy).count }
}

struct ResultadoEjecucionMigracionSidecarsIPad: Equatable {
    var documentoId: DocumentoPersistenteID
    var notasAdhesivas: ResultadoMigracionSidecarIPad?
    var recovery: ResultadoMigracionDirectorioIPad?
    var advertencias: [String]
}

/// Catálogo mínimo de rutas legacy confirmadas en el repo. No serializa ni
/// interpreta los modelos de sus owners: sólo migra su identidad de filesystem.
enum PlanMigracionSidecarsConocidosIPad {
    static func construir(
        documentoId: DocumentoPersistenteID,
        archivos: FileManager = .default
    ) -> ReportePlanMigracionSidecarsIPad {
        let soporte = archivos.urls(for: .applicationSupportDirectory,
                                    in: .userDomainMask)[0]
        let documentos = archivos.urls(for: .documentDirectory,
                                       in: .userDomainMask)[0]

        let notasBase = soporte.appendingPathComponent(
            "Tinta/NotasAdhesivas", isDirectory: true)
        let notas = DescriptorSidecarFilePerDocumentIPad(
            nombre: "Notas adhesivas",
            carpeta: notasBase)
        let rutasNotas = notas.rutas(documentoId: documentoId)

        let recoveryBase = documentos.appendingPathComponent(
            "Recuperacion", isDirectory: true)
        let recoveryLegacy = recoveryBase.appendingPathComponent(
            String(documentoId.claveCompatibilidadUInt64), isDirectory: true)
        let recoveryDurable = recoveryBase.appendingPathComponent(
            documentoId.description, isDirectory: true)

        return .init(documentoId: documentoId, pasos: [
            .init(
                nombre: "Notas adhesivas",
                tipo: .archivoPorDocumento,
                origen: rutasNotas.legacy,
                destino: rutasNotas.durable,
                existeLegacy: archivos.fileExists(atPath: rutasNotas.legacy.path),
                existeDurable: archivos.fileExists(atPath: rutasNotas.durable.path)),
            .init(
                nombre: "Recovery / autoguardado",
                tipo: .directorioPorDocumento,
                origen: recoveryLegacy,
                destino: recoveryDurable,
                existeLegacy: archivos.fileExists(atPath: recoveryLegacy.path),
                existeDurable: archivos.fileExists(atPath: recoveryDurable.path))
        ])
    }

    /// Sólo ejecuta pasos cuyo origen legacy existe. Un conflicto en un paso no
    /// hace que el siguiente se ejecute silenciosamente; se detiene y conserva
    /// todo para diagnóstico, porque ambas rutas pertenecen al mismo documento.
    static func ejecutar(
        documentoId: DocumentoPersistenteID,
        transacciones: CoordinadorPersistenciaTransaccionalIPad = .compartido,
        archivos: FileManager = .default
    ) throws -> ResultadoEjecucionMigracionSidecarsIPad {
        let plan = construir(documentoId: documentoId, archivos: archivos)
        var notasResultado: ResultadoMigracionSidecarIPad?
        var recoveryResultado: ResultadoMigracionDirectorioIPad?
        var advertencias: [String] = []

        for paso in plan.pasos where paso.existeLegacy {
            switch paso.tipo {
            case .archivoPorDocumento:
                notasResultado = try MigradorIdentidadSidecarsIPad(
                    transacciones: transacciones,
                    archivos: archivos).migrarArchivo(
                        documentoId: documentoId,
                        claveLegacy: documentoId.claveCompatibilidadUInt64,
                        repositorio: paso.nombre,
                        origenLegacy: paso.origen,
                        destinoDurable: paso.destino)

            case .directorioPorDocumento:
                recoveryResultado = try MigradorDirectorioIdentidadIPad.migrar(
                    documentoId: documentoId,
                    claveLegacy: documentoId.claveCompatibilidadUInt64,
                    origenLegacy: paso.origen,
                    destinoDurable: paso.destino,
                    transacciones: transacciones,
                    archivos: archivos)
            }
        }

        if plan.legacyEncontrados == 0 {
            advertencias.append("No se encontraron sidecars legacy en las rutas conocidas.")
        }
        return .init(documentoId: documentoId,
                     notasAdhesivas: notasResultado,
                     recovery: recoveryResultado,
                     advertencias: advertencias)
    }
}
