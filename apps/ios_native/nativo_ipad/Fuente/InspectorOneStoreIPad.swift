import Foundation

/// Inspección estructural mínima y estricta del encabezado MS-ONESTORE.
/// Esta capa NO interpreta todavía object spaces ni property sets; su trabajo es impedir que
/// un archivo renombrado/corrupto avance al parser semántico.
struct InspeccionOneStore: Sendable, Equatable {
    enum TipoRevisionStore: String, Sendable, Equatable {
        case seccionOne
        case tablaContenidosOneToc2
    }

    var tipo: TipoRevisionStore
    var guidArchivo: UUID
    var guidFormato: UUID
    var versionUltimoEscritor: UInt32
    var versionMasAntiguaEscritora: UInt32
    var versionMasNuevaEscritora: UInt32
    var versionMasAntiguaLectora: UInt32
    var tamanoBytes: UInt64
}

enum ErrorInspeccionOneStore: Error, LocalizedError, Equatable, Sendable {
    case archivoDemasiadoCorto(Int)
    case guidTipoDesconocido(UUID)
    case guidFormatoInvalido(UUID)
    case versionIncompatible(tipo: String, encontrada: UInt32, esperada: UInt32)
    case extensionNoCoincide(tipo: String, extensionArchivo: String)

    var errorDescription: String? {
        switch self {
        case .archivoDemasiadoCorto(let bytes):
            return "El archivo no contiene un encabezado MS-ONESTORE completo (\(bytes) bytes disponibles)."
        case .guidTipoDesconocido(let guid):
            return "El GUID de tipo MS-ONESTORE no corresponde a .one ni .onetoc2: \(guid.uuidString)."
        case .guidFormatoInvalido(let guid):
            return "El GUID de formato no identifica un OneNote Revision Store: \(guid.uuidString)."
        case .versionIncompatible(let tipo, let encontrada, let esperada):
            return "La versión de encabezado \(tipo) es \(encontrada); se esperaba \(esperada)."
        case .extensionNoCoincide(let tipo, let ext):
            return "El contenido es \(tipo), pero la extensión del archivo es .\(ext)."
        }
    }
}

struct LectorBinarioOneStore: Sendable {
    let datos: Data

    func uint32LE(offset: Int) throws -> UInt32 {
        guard offset >= 0, offset + 4 <= datos.count else {
            throw ErrorInspeccionOneStore.archivoDemasiadoCorto(datos.count)
        }
        return UInt32(datos[offset]) |
            (UInt32(datos[offset + 1]) << 8) |
            (UInt32(datos[offset + 2]) << 16) |
            (UInt32(datos[offset + 3]) << 24)
    }

    /// GUID en representación binaria Windows/MS-DTYP: Data1/Data2/Data3 little-endian,
    /// los ocho bytes finales en orden de red.
    func guid(offset: Int) throws -> UUID {
        guard offset >= 0, offset + 16 <= datos.count else {
            throw ErrorInspeccionOneStore.archivoDemasiadoCorto(datos.count)
        }
        let b = Array(datos[offset..<(offset + 16)])
        return UUID(uuid: (
            b[3], b[2], b[1], b[0],
            b[5], b[4],
            b[7], b[6],
            b[8], b[9], b[10], b[11],
            b[12], b[13], b[14], b[15]
        ))
    }
}

enum InspectorOneStoreIPad {
    // MS-ONESTORE 2.3.1 Header
    static let guidTipoOne = UUID(uuidString: "7B5C52E4-D88C-4DA7-AEB1-5378D02996D3")!
    static let guidTipoOneToc2 = UUID(uuidString: "43FF2FA1-EFD9-4C76-9EE2-10EA5722765F")!
    static let guidRevisionStore = UUID(uuidString: "109ADD3F-911B-49F5-A5D0-1791EDC8AED8")!

    static func inspeccionar(url: URL, validarExtension: Bool = true) throws -> InspeccionOneStore {
        let atributos = try FileManager.default.attributesOfItem(atPath: url.path)
        let tamano = (atributos[.size] as? NSNumber)?.uint64Value ?? 0

        let handle = try FileHandle(forReadingFrom: url)
        defer { try? handle.close() }
        let prefijo = try handle.read(upToCount: 80) ?? Data()
        return try inspeccionar(
            prefijo: prefijo,
            tamanoTotal: tamano,
            extensionArchivo: validarExtension ? url.pathExtension.lowercased() : nil
        )
    }

    static func inspeccionar(
        prefijo: Data,
        tamanoTotal: UInt64,
        extensionArchivo: String? = nil
    ) throws -> InspeccionOneStore {
        // Necesitamos GUIDs de tipo/archivo/formato y cuatro FFV (hasta offset 0x50).
        guard prefijo.count >= 80 else {
            throw ErrorInspeccionOneStore.archivoDemasiadoCorto(prefijo.count)
        }

        let lector = LectorBinarioOneStore(datos: prefijo)
        let guidTipo = try lector.guid(offset: 0x00)
        let guidArchivo = try lector.guid(offset: 0x10)
        let guidFormato = try lector.guid(offset: 0x30)

        guard guidFormato == guidRevisionStore else {
            throw ErrorInspeccionOneStore.guidFormatoInvalido(guidFormato)
        }

        let tipo: InspeccionOneStore.TipoRevisionStore
        let versionEsperada: UInt32
        let extensionEsperada: String
        if guidTipo == guidTipoOne {
            tipo = .seccionOne
            versionEsperada = 0x2A
            extensionEsperada = "one"
        } else if guidTipo == guidTipoOneToc2 {
            tipo = .tablaContenidosOneToc2
            versionEsperada = 0x1B
            extensionEsperada = "onetoc2"
        } else {
            throw ErrorInspeccionOneStore.guidTipoDesconocido(guidTipo)
        }

        if let extensionArchivo, !extensionArchivo.isEmpty, extensionArchivo != extensionEsperada {
            throw ErrorInspeccionOneStore.extensionNoCoincide(
                tipo: extensionEsperada,
                extensionArchivo: extensionArchivo
            )
        }

        let ultimo = try lector.uint32LE(offset: 0x40)
        let antiguoEscritor = try lector.uint32LE(offset: 0x44)
        let nuevoEscritor = try lector.uint32LE(offset: 0x48)
        let antiguoLector = try lector.uint32LE(offset: 0x4C)

        for (nombre, valor) in [
            ("ffvLastCodeThatWroteToThisFile", ultimo),
            ("ffvOldestCodeThatHasWrittenToThisFile", antiguoEscritor),
            ("ffvNewestCodeThatHasWrittenToThisFile", nuevoEscritor),
            ("ffvOldestCodeThatMayReadThisFile", antiguoLector)
        ] where valor != versionEsperada {
            throw ErrorInspeccionOneStore.versionIncompatible(
                tipo: nombre,
                encontrada: valor,
                esperada: versionEsperada
            )
        }

        return InspeccionOneStore(
            tipo: tipo,
            guidArchivo: guidArchivo,
            guidFormato: guidFormato,
            versionUltimoEscritor: ultimo,
            versionMasAntiguaEscritora: antiguoEscritor,
            versionMasNuevaEscritora: nuevoEscritor,
            versionMasAntiguaLectora: antiguoLector,
            tamanoBytes: tamanoTotal
        )
    }
}
