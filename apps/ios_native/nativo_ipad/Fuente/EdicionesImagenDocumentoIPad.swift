import Foundation

/// Espacio documental para los filtros no destructivos de imágenes.
///
/// El repositorio histórico indexa únicamente por `medio.id`. Eso era suficiente
/// mientras un ID aparecía en un solo documento, pero `cargarComoCopia` conserva
/// deliberadamente los IDs internos para que semántica, Recall y referencias
/// sigan apuntando al mismo elemento lógico. Por tanto una copia puede contener
/// el mismo `medio.id` que el original y no debe compartir su edición mutable.
///
/// Este repositorio añade la identidad durable del documento a la clave sin
/// destruir el archivo histórico. La primera lectura de una entrada todavía no
/// migrada adopta el valor legacy y lo persiste inmediatamente bajo el documento;
/// desde ese momento original y copia evolucionan de forma independiente.
final class RepositorioEdicionesImagenDocumentoIPad {
    static let compartido = RepositorioEdicionesImagenDocumentoIPad()

    private struct Archivo: Codable {
        var formato = 1
        var porDocumento: [String: [String: AjustesEdicionImagen]] = [:]
    }

    private let cola = DispatchQueue(label: "tinta.ediciones-imagen.documento")
    private let url: URL
    private var archivo: Archivo

    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask).first!
        let carpeta = documentos.appendingPathComponent("Metadatos", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("ediciones-imagen-documento.json")
        archivo = (try? Data(contentsOf: url)).flatMap {
            try? JSONDecoder().decode(Archivo.self, from: $0)
        } ?? Archivo()
    }

    private func clave(_ documento: UInt64) -> String { String(documento) }

    private static func limpiarContexto(_ valor: AjustesEdicionImagen)
        -> AjustesEdicionImagen {
        var limpio = valor
        limpio.documentoContexto = nil
        return limpio
    }

    /// Devuelve la edición efectiva y materializa una copia documental cuando
    /// solo existe el sidecar legacy. Persistir incluso el valor por defecto es
    /// intencional: evita que una escritura legacy posterior cambie una copia
    /// que ya fue abierta con su propia identidad durable.
    func ajustes(documento: UInt64, id: String) -> AjustesEdicionImagen {
        cola.sync {
            let documentoClave = clave(documento)
            if let valor = archivo.porDocumento[documentoClave]?[id] {
                return Self.limpiarContexto(valor)
            }

            let heredado = Self.limpiarContexto(
                RepositorioEdicionesImagen.compartido.ajustesLegacy(id: id))
            var siguiente = archivo
            var valores = siguiente.porDocumento[documentoClave] ?? [:]
            valores[id] = heredado
            siguiente.porDocumento[documentoClave] = valores
            do {
                try persistir(siguiente)
                archivo = siguiente
            } catch {
                // La lectura puede continuar con el valor efectivo aunque el
                // disco esté temporalmente indisponible. Una edición explícita
                // sí devuelve `false` y no publica memoria adelantada.
            }
            return heredado
        }
    }

    /// Compatibilidad controlada para callbacks legacy asíncronos. Solo se
    /// devuelve un documento cuando el ID está materializado en exactamente un
    /// namespace. Si original y copia comparten el ID, el resultado es `nil` y
    /// el consumidor necesita un contexto explícito.
    func documentoUnico(materializadoPara id: String) -> UInt64? {
        cola.sync {
            var encontrado: UInt64?
            for (documentoClave, valores) in archivo.porDocumento where valores[id] != nil {
                guard let documento = UInt64(documentoClave) else { continue }
                if encontrado != nil { return nil }
                encontrado = documento
            }
            return encontrado
        }
    }

    @discardableResult
    func guardar(_ ajustes: AjustesEdicionImagen,
                 documento: UInt64,
                 id: String) -> Bool {
        cola.sync {
            let documentoClave = clave(documento)
            var siguiente = archivo
            var valores = siguiente.porDocumento[documentoClave] ?? [:]
            valores[id] = Self.limpiarContexto(ajustes)
            siguiente.porDocumento[documentoClave] = valores
            do {
                try persistir(siguiente)
                archivo = siguiente
                return true
            } catch {
                return false
            }
        }
    }

    /// Duplica un medio dentro del mismo documento. El nuevo ID recibe una
    /// fotografía independiente de los ajustes efectivos del elemento fuente.
    @discardableResult
    func duplicarElemento(documento: UInt64,
                          origen: String,
                          destino: String) -> Bool {
        guard origen != destino else { return false }
        let valor = ajustes(documento: documento, id: origen)
        return guardar(valor, documento: documento, id: destino)
    }

    /// Copia todas las ediciones que ya pertenecen explícitamente al documento.
    /// Si una nota antigua todavía vive solo en el sidecar legacy, no se inventa
    /// una pertenencia: la primera lectura en origen y copia materializará el
    /// mismo valor inicial en namespaces separados y desde ahí evolucionarán de
    /// manera independiente.
    @discardableResult
    func duplicar(desde origen: UInt64, hacia destino: UInt64) throws -> Int {
        guard origen != destino else { throw CocoaError(.fileWriteInvalidFileName) }
        return try cola.sync {
            let origenClave = clave(origen)
            let destinoClave = clave(destino)
            let existente = archivo.porDocumento[destinoClave] ?? [:]
            guard existente.isEmpty else { throw CocoaError(.fileWriteFileExists) }

            let fuente = archivo.porDocumento[origenClave] ?? [:]
            let copia = fuente.mapValues(Self.limpiarContexto)
            var siguiente = archivo
            siguiente.porDocumento[destinoClave] = copia
            try persistir(siguiente)
            archivo = siguiente
            return copia.count
        }
    }

    /// Útil para limpieza de una pestaña descartada o una compensación de una
    /// operación mayor. El archivo legacy no se toca porque no tiene identidad
    /// documental suficiente para atribuir sus entradas con seguridad.
    func eliminarDocumento(_ documento: UInt64) throws {
        try cola.sync {
            let documentoClave = clave(documento)
            guard archivo.porDocumento[documentoClave] != nil else { return }
            var siguiente = archivo
            siguiente.porDocumento.removeValue(forKey: documentoClave)
            try persistir(siguiente)
            archivo = siguiente
        }
    }

    /// Expone únicamente la instantánea ya materializada del documento. No
    /// fuerza migraciones legacy y sirve para pruebas de aislamiento/rollback.
    func instantanea(documento: UInt64) -> [String: AjustesEdicionImagen] {
        cola.sync {
            (archivo.porDocumento[clave(documento)] ?? [:])
                .mapValues(Self.limpiarContexto)
        }
    }

    private func persistir(_ valor: Archivo) throws {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.sortedKeys]
        let datos = try encoder.encode(valor)
        try datos.write(to: url, options: [.atomic, .completeFileProtection])
    }
}
