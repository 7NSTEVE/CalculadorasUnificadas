import Foundation

/// Overlay transaccional para la procedencia PPTX de documentos duplicados.
///
/// `RepositorioObjetosPPTXIPad` ya separa su diccionario por documento, por lo
/// que no padece la colisión global de imágenes/clips/texto. Sin embargo su API
/// legacy no permite enumerar, duplicar ni eliminar un namespace con confirmación
/// de errores. Esta capa conserva las copias en un archivo propio y consulta el
/// repositorio original como fallback para documentos no duplicados.
///
/// Los UUID de diapositiva y de `ElementoPPTXIPad` NO se regeneran: describen la
/// fuente inmutable de la que salió el objeto. La identidad que cambia es la del
/// documento que contiene el objeto nativo.
final class RepositorioProcedenciaPPTXCopiaIPad {
    static let compartido = RepositorioProcedenciaPPTXCopiaIPad()

    private struct Archivo: Codable {
        var formato = 1
        var documentos: [String: [String: MetadatoObjetoPPTXIPad]] = [:]
    }

    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private let urlLegacy: URL
    private var archivo: Archivo

    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask)[0]
        let carpeta = documentos.appendingPathComponent(
            "Academico/PPTXObjetos", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("procedencia-copias.json")
        urlLegacy = carpeta.appendingPathComponent("procedencia.json")
        archivo = (try? Data(contentsOf: url)).flatMap {
            try? JSONDecoder().decode(Archivo.self, from: $0)
        } ?? Archivo()
    }

    private func clave(_ documento: UInt64) -> String { String(documento) }

    /// Punto de lectura que deben usar UI y exportadores. Primero consulta la
    /// fotografía transaccional de una copia; si no existe esa clave, conserva
    /// compatibilidad con objetos registrados por el flujo PPTX original.
    func metadato(objetoId: String, documento: UInt64) -> MetadatoObjetoPPTXIPad? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        if let copiado = archivo.documentos[clave(documento)]?[objetoId] {
            return copiado
        }
        return RepositorioObjetosPPTXIPad.compartido.metadato(
            objetoId: objetoId, documento: documento)
    }

    /// Escritura fuerte para metadata que ya debe pertenecer al overlay. Sirve
    /// para pruebas, migraciones y futuros flujos que quieran evitar el escritor
    /// legacy que actualmente silencia errores de disco.
    func registrar(_ metadato: MetadatoObjetoPPTXIPad,
                   objetoId: String,
                   documento: UInt64) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let documentoClave = clave(documento)
        var siguiente = archivo
        var valores = siguiente.documentos[documentoClave] ?? [:]
        valores[objetoId] = metadato
        siguiente.documentos[documentoClave] = valores
        try persistir(siguiente)
        archivo = siguiente
    }

    /// Reúne la vista efectiva del documento: metadata legacy registrada al
    /// separar PPTX más cualquier fotografía proveniente de una duplicación
    /// anterior. Esto hace que copiar una copia sea transitivo.
    func instantaneaEfectiva(documento: UInt64) throws
        -> [String: MetadatoObjetoPPTXIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        var salida = try legacy(documento: documento)
        if let copiada = archivo.documentos[clave(documento)] {
            salida.merge(copiada) { _, copia in copia }
        }
        return salida
    }

    @discardableResult
    func duplicar(desde origen: UInt64, hacia destino: UInt64) throws -> Int {
        guard origen != destino else { throw CocoaError(.fileWriteInvalidFileName) }
        bloqueo.lock(); defer { bloqueo.unlock() }

        let destinoClave = clave(destino)
        let existente = archivo.documentos[destinoClave] ?? [:]
        guard existente.isEmpty,
              try legacy(documento: destino).isEmpty else {
            throw CocoaError(.fileWriteFileExists)
        }

        var fuente = try legacy(documento: origen)
        if let copiada = archivo.documentos[clave(origen)] {
            fuente.merge(copiada) { _, copia in copia }
        }
        guard !fuente.isEmpty else { return 0 }

        var siguiente = archivo
        siguiente.documentos[destinoClave] = fuente
        try persistir(siguiente)
        archivo = siguiente
        return fuente.count
    }

    func eliminarDocumento(_ documento: UInt64) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let documentoClave = clave(documento)
        guard archivo.documentos[documentoClave] != nil else { return }
        var siguiente = archivo
        siguiente.documentos.removeValue(forKey: documentoClave)
        try persistir(siguiente)
        archivo = siguiente
    }

    /// Solo el overlay, útil para comprobar rollback sin mezclar el repositorio
    /// legacy que podría contener objetos creados legítimamente en ese documento.
    func instantaneaCopiada(documento: UInt64) -> [String: MetadatoObjetoPPTXIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return archivo.documentos[clave(documento)] ?? [:]
    }

    private func legacy(documento: UInt64) throws
        -> [String: MetadatoObjetoPPTXIPad] {
        guard FileManager.default.fileExists(atPath: urlLegacy.path) else { return [:] }
        let datos = try Data(contentsOf: urlLegacy)
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        let todos = try decoder.decode(
            [String: [String: MetadatoObjetoPPTXIPad]].self, from: datos)
        return todos[clave(documento)] ?? [:]
    }

    private func persistir(_ valor: Archivo) throws {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        let datos = try encoder.encode(valor)
        try datos.write(to: url, options: [.atomic, .completeFileProtection])
    }
}
