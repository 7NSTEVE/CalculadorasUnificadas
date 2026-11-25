import Foundation

struct ReporteLimpiezaTransaccionesHuerfanasIPad: Equatable {
    var carpetasInspeccionadas = 0
    var huerfanasEliminadas: [String] = []
    var recientesConservadas: [String] = []
    var conJournalConservadas: [String] = []
    var errores: [String] = []
}

/// Limpia únicamente directorios de prepare que nunca llegaron a publicar
/// `journal.json`. El coordinador P0 no modifica destinos antes de que exista ese
/// journal, por lo que un directorio sin journal es sólo staging/backup huérfano.
/// Se exige además una antigüedad mínima para no competir con un prepare activo.
enum LimpiadorTransaccionesHuerfanasIPad {
    static func ejecutar(
        base: URL? = nil,
        antiguedadMinima: TimeInterval = 15 * 60,
        ahora: Date = Date(),
        archivos: FileManager = .default
    ) -> ReporteLimpiezaTransaccionesHuerfanasIPad {
        let carpetaBase = base ?? archivos.urls(
            for: .applicationSupportDirectory,
            in: .userDomainMask)[0]
            .appendingPathComponent("Persistencia/Transacciones", isDirectory: true)
        var reporte = ReporteLimpiezaTransaccionesHuerfanasIPad()
        let propiedades: Set<URLResourceKey> = [
            .isDirectoryKey, .contentModificationDateKey, .creationDateKey
        ]
        let candidatos = (try? archivos.contentsOfDirectory(
            at: carpetaBase,
            includingPropertiesForKeys: Array(propiedades),
            options: [.skipsHiddenFiles])) ?? []

        for candidato in candidatos {
            guard (try? candidato.resourceValues(forKeys: [.isDirectoryKey]))?.isDirectory == true else { continue }
            reporte.carpetasInspeccionadas += 1
            let nombre = candidato.lastPathComponent
            let journal = candidato.appendingPathComponent("journal.json")
            if archivos.fileExists(atPath: journal.path) {
                reporte.conJournalConservadas.append(nombre)
                continue
            }

            let valores = try? candidato.resourceValues(forKeys: propiedades)
            let fecha = valores?.contentModificationDate
                ?? valores?.creationDate
                ?? .distantPast
            let edad = ahora.timeIntervalSince(fecha)
            guard edad >= max(antiguedadMinima, 0) else {
                reporte.recientesConservadas.append(nombre)
                continue
            }

            do {
                try archivos.removeItem(at: candidato)
                reporte.huerfanasEliminadas.append(nombre)
            } catch {
                reporte.errores.append("\(nombre): \(error.localizedDescription)")
            }
        }

        reporte.huerfanasEliminadas.sort()
        reporte.recientesConservadas.sort()
        reporte.conJournalConservadas.sort()
        reporte.errores.sort()
        return reporte
    }
}
