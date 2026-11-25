import Foundation

struct RangoTextoEnlaceIPad: Codable, Equatable, Hashable {
    var localizacionUTF16: Int
    var longitudUTF16: Int
}

enum TipoDestinoEnlaceIPad: String, Codable, Equatable, Hashable {
    case web
    case documento
    case problema
    case formula
    case fuente
}

struct DestinoEnlaceIPad: Codable, Equatable, Hashable {
    var tipo: TipoDestinoEnlaceIPad
    var clave: String
    var documentoId: DocumentoPersistenteID?
    var pagina: Int?
    var frameId: UUID?

    static func web(_ url: URL) -> DestinoEnlaceIPad {
        .init(tipo: .web, clave: url.absoluteString)
    }
    static func documento(_ id: DocumentoPersistenteID, pagina: Int? = nil,
                          frameId: UUID? = nil) -> DestinoEnlaceIPad {
        .init(tipo: .documento, clave: id.description,
              documentoId: id, pagina: pagina, frameId: frameId)
    }
    static func problema(_ id: UUID) -> DestinoEnlaceIPad {
        .init(tipo: .problema, clave: id.uuidString.lowercased())
    }
    static func formula(_ id: UUID) -> DestinoEnlaceIPad {
        .init(tipo: .formula, clave: id.uuidString.lowercased())
    }
    static func fuente(_ clave: String) -> DestinoEnlaceIPad {
        .init(tipo: .fuente, clave: clave)
    }

    var claveBacklink: String { "\(tipo.rawValue):\(clave)" }
}

enum ErrorCodecEnlaceInternoIPad: Error, Equatable {
    case esquemaInvalido
    case hostInvalido
    case parametrosDuplicados
    case parametroDesconocido(String)
    case versionNoSoportada
    case tipoInvalido
    case destinoWebNoInterno
    case claveFaltante
    case documentoInvalido
    case paginaInvalida
    case frameInvalido
    case destinoInconsistente
}

enum CodecEnlacesInternosIPad {
    static let esquema = "tinta"
    static let host = "abrir"
    static let version = 1

    private static let parametrosPermitidos: Set<String> = [
        "v", "tipo", "clave", "documento", "pagina", "frame"
    ]

    static func crearURL(para destino: DestinoEnlaceIPad) throws -> URL {
        guard destino.tipo != .web else {
            throw ErrorCodecEnlaceInternoIPad.destinoWebNoInterno
        }
        try validar(destino)
        var componentes = URLComponents()
        componentes.scheme = esquema
        componentes.host = host
        var items: [URLQueryItem] = [
            .init(name: "v", value: String(version)),
            .init(name: "tipo", value: destino.tipo.rawValue),
            .init(name: "clave", value: destino.clave)
        ]
        if let documento = destino.documentoId {
            items.append(.init(name: "documento", value: documento.description))
        }
        if let pagina = destino.pagina {
            items.append(.init(name: "pagina", value: String(pagina)))
        }
        if let frame = destino.frameId {
            items.append(.init(name: "frame", value: frame.uuidString.lowercased()))
        }
        componentes.queryItems = items
        guard let url = componentes.url else {
            throw ErrorCodecEnlaceInternoIPad.destinoInconsistente
        }
        return url
    }

    static func decodificar(_ url: URL) throws -> DestinoEnlaceIPad {
        guard url.scheme?.lowercased() == esquema else {
            throw ErrorCodecEnlaceInternoIPad.esquemaInvalido
        }
        guard url.host?.lowercased() == host else {
            throw ErrorCodecEnlaceInternoIPad.hostInvalido
        }
        guard let componentes = URLComponents(url: url, resolvingAgainstBaseURL: false) else {
            throw ErrorCodecEnlaceInternoIPad.destinoInconsistente
        }
        let items = componentes.queryItems ?? []
        let nombres = items.map(\.name)
        guard Set(nombres).count == nombres.count else {
            throw ErrorCodecEnlaceInternoIPad.parametrosDuplicados
        }
        if let desconocido = nombres.first(where: { !parametrosPermitidos.contains($0) }) {
            throw ErrorCodecEnlaceInternoIPad.parametroDesconocido(desconocido)
        }
        let mapa = Dictionary(uniqueKeysWithValues: items.map { ($0.name, $0.value ?? "") })
        guard mapa["v"] == String(version) else {
            throw ErrorCodecEnlaceInternoIPad.versionNoSoportada
        }
        guard let tipoBruto = mapa["tipo"],
              let tipo = TipoDestinoEnlaceIPad(rawValue: tipoBruto) else {
            throw ErrorCodecEnlaceInternoIPad.tipoInvalido
        }
        guard tipo != .web else {
            throw ErrorCodecEnlaceInternoIPad.destinoWebNoInterno
        }
        guard let clave = mapa["clave"], !clave.isEmpty else {
            throw ErrorCodecEnlaceInternoIPad.claveFaltante
        }
        var documento: DocumentoPersistenteID?
        if let bruto = mapa["documento"] {
            guard let uuid = UUID(uuidString: bruto) else {
                throw ErrorCodecEnlaceInternoIPad.documentoInvalido
            }
            documento = DocumentoPersistenteID(uuid)
        }
        var pagina: Int?
        if let bruto = mapa["pagina"] {
            guard let valor = Int(bruto), valor >= 0 else {
                throw ErrorCodecEnlaceInternoIPad.paginaInvalida
            }
            pagina = valor
        }
        var frame: UUID?
        if let bruto = mapa["frame"] {
            guard let valor = UUID(uuidString: bruto) else {
                throw ErrorCodecEnlaceInternoIPad.frameInvalido
            }
            frame = valor
        }
        let destino = DestinoEnlaceIPad(
            tipo: tipo, clave: clave, documentoId: documento,
            pagina: pagina, frameId: frame)
        try validar(destino)
        return destino
    }

    static func esEnlaceInterno(_ url: URL) -> Bool {
        url.scheme?.lowercased() == esquema && url.host?.lowercased() == host
    }

    private static func validar(_ destino: DestinoEnlaceIPad) throws {
        guard !destino.clave.isEmpty else {
            throw ErrorCodecEnlaceInternoIPad.claveFaltante
        }
        if let pagina = destino.pagina, pagina < 0 {
            throw ErrorCodecEnlaceInternoIPad.paginaInvalida
        }
        switch destino.tipo {
        case .web:
            throw ErrorCodecEnlaceInternoIPad.destinoWebNoInterno
        case .documento:
            guard let documento = destino.documentoId,
                  destino.clave.lowercased() == documento.description.lowercased() else {
                throw ErrorCodecEnlaceInternoIPad.destinoInconsistente
            }
        case .problema, .formula:
            guard UUID(uuidString: destino.clave) != nil else {
                throw ErrorCodecEnlaceInternoIPad.destinoInconsistente
            }
        case .fuente:
            break
        }
    }
}

struct EnlaceAutomaticoIPad: Codable, Equatable, Identifiable {
    var id: String {
        "\(rango.localizacionUTF16):\(rango.longitudUTF16):\(url.absoluteString)"
    }
    var texto: String
    var url: URL
    var rango: RangoTextoEnlaceIPad
}

enum DetectorEnlacesAutomaticosIPad {
    private static let regex = try! NSRegularExpression(
        pattern: #"(?i)\b((?:https?://|www\.|tinta://)[^\s<>{}\[\]\"']+)"#)

    static func detectar(en texto: String) -> [EnlaceAutomaticoIPad] {
        let ns = texto as NSString
        let rango = NSRange(location: 0, length: ns.length)
        return regex.matches(in: texto, range: rango).compactMap { coincidencia in
            var bruto = ns.substring(with: coincidencia.range(at: 1))
            while let ultimo = bruto.last, ".,;:!?)]".contains(ultimo) {
                bruto.removeLast()
            }
            let normalizado = bruto.lowercased().hasPrefix("www.")
                ? "https://\(bruto)" : bruto
            guard let url = URL(string: normalizado) else { return nil }
            if CodecEnlacesInternosIPad.esEnlaceInterno(url) {
                let nuevoValido = (try? CodecEnlacesInternosIPad.decodificar(url)) != nil
                let legacyValido = DestinoEnlaceTinta.leer(url) != nil
                guard nuevoValido || legacyValido else { return nil }
            }
            return EnlaceAutomaticoIPad(
                texto: bruto, url: url,
                rango: .init(localizacionUTF16: coincidencia.range(at: 1).location,
                             longitudUTF16: (bruto as NSString).length))
        }
    }
}

struct BacklinkCuadernUIPad: Codable, Equatable, Identifiable {
    var id: UUID
    var destino: DestinoEnlaceIPad
    var documentoOrigen: DocumentoPersistenteID
    var paginaOrigen: Int?
    var frameOrigen: UUID?
    var etiqueta: String
    var contexto: String
    var fecha: Date

    init(id: UUID = UUID(), destino: DestinoEnlaceIPad,
         documentoOrigen: DocumentoPersistenteID, paginaOrigen: Int? = nil,
         frameOrigen: UUID? = nil, etiqueta: String = "", contexto: String = "",
         fecha: Date = Date()) {
        self.id = id
        self.destino = destino
        self.documentoOrigen = documentoOrigen
        self.paginaOrigen = paginaOrigen
        self.frameOrigen = frameOrigen
        self.etiqueta = etiqueta
        self.contexto = contexto
        self.fecha = fecha
    }
}

private struct ClaveSemanticaBacklinkIPad: Hashable {
    var destino: DestinoEnlaceIPad
    var documentoOrigen: DocumentoPersistenteID
    var paginaOrigen: Int?
    var frameOrigen: UUID?
}

private extension BacklinkCuadernUIPad {
    var claveSemantica: ClaveSemanticaBacklinkIPad {
        .init(destino: destino, documentoOrigen: documentoOrigen,
              paginaOrigen: paginaOrigen, frameOrigen: frameOrigen)
    }
}

private struct ArchivoBacklinksIPad: Codable {
    var version = 1
    var backlinks: [BacklinkCuadernUIPad] = []
}

final class RepositorioBacklinksIPad {
    static let compartido = RepositorioBacklinksIPad()
    private let url: URL
    private let cola = DispatchQueue(label: "cuadernu.backlinks")

    init(base: URL? = nil, archivos: FileManager = .default) {
        let carpeta: URL
        if let base {
            carpeta = base
        } else {
            carpeta = archivos.urls(for: .applicationSupportDirectory,
                                    in: .userDomainMask)[0]
                .appendingPathComponent("Academico/Backlinks", isDirectory: true)
        }
        try? archivos.createDirectory(at: carpeta, withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("backlinks.json")
    }

    func backlinks(hacia destino: DestinoEnlaceIPad) -> [BacklinkCuadernUIPad] {
        cola.sync {
            deduplicar(cargar().backlinks)
                .filter { $0.destino == destino }
                .sorted { $0.fecha > $1.fecha }
        }
    }

    /// Backlinks hacia cualquier ancla del documento, incluyendo página/frame.
    func backlinks(haciaDocumento documento: DocumentoPersistenteID)
        -> [BacklinkCuadernUIPad] {
        cola.sync {
            deduplicar(cargar().backlinks)
                .filter { $0.destino.documentoId == documento }
                .sorted { $0.fecha > $1.fecha }
        }
    }

    func salientes(desde documento: DocumentoPersistenteID) -> [BacklinkCuadernUIPad] {
        cola.sync {
            deduplicar(cargar().backlinks)
                .filter { $0.documentoOrigen == documento }
                .sorted { $0.fecha > $1.fecha }
        }
    }

    @discardableResult
    func upsert(_ backlink: BacklinkCuadernUIPad) -> Bool {
        cola.sync {
            var archivo = cargar()
            if let indicePorId = archivo.backlinks.firstIndex(where: { $0.id == backlink.id }) {
                archivo.backlinks[indicePorId] = backlink
            } else if let indiceSemantico = archivo.backlinks.firstIndex(where: {
                $0.claveSemantica == backlink.claveSemantica
            }) {
                let anterior = archivo.backlinks[indiceSemantico]
                var fusionado = backlink
                fusionado.id = anterior.id
                fusionado.fecha = anterior.fecha
                archivo.backlinks[indiceSemantico] = fusionado
            } else {
                archivo.backlinks.append(backlink)
            }
            archivo.backlinks = deduplicar(archivo.backlinks)
            return guardar(archivo)
        }
    }

    /// Reemplaza sólo las aristas del documento. Preserva ID/fecha de una arista
    /// semánticamente idéntica y evita I/O si el snapshot resultante no cambió.
    @discardableResult
    func reemplazarSalientes(
        de documento: DocumentoPersistenteID,
        con nuevos: [BacklinkCuadernUIPad]
    ) -> Bool {
        guard nuevos.allSatisfy({ $0.documentoOrigen == documento }) else {
            return false
        }
        return cola.sync {
            let archivo = cargar()
            let actuales = deduplicar(archivo.backlinks)
            let previosDocumento = actuales.filter { $0.documentoOrigen == documento }
            let previosPorClave = Dictionary(
                uniqueKeysWithValues: previosDocumento.map { ($0.claveSemantica, $0) })

            let normalizados = deduplicar(nuevos).map { nuevo -> BacklinkCuadernUIPad in
                guard let previo = previosPorClave[nuevo.claveSemantica] else {
                    return nuevo
                }
                var fusionado = nuevo
                fusionado.id = previo.id
                fusionado.fecha = previo.fecha
                return fusionado
            }
            let candidato = actuales.filter { $0.documentoOrigen != documento }
                + normalizados
            guard candidato != actuales else { return true }
            return guardar(ArchivoBacklinksIPad(
                version: archivo.version,
                backlinks: candidato))
        }
    }

    @discardableResult
    func eliminar(_ id: UUID) -> Bool {
        cola.sync {
            var archivo = cargar()
            let cantidadAnterior = archivo.backlinks.count
            archivo.backlinks.removeAll { $0.id == id }
            guard archivo.backlinks.count != cantidadAnterior else { return true }
            return guardar(archivo)
        }
    }

    @discardableResult
    func eliminar(origen documento: DocumentoPersistenteID,
                  destino: DestinoEnlaceIPad) -> Bool {
        cola.sync {
            var archivo = cargar()
            archivo.backlinks.removeAll {
                $0.documentoOrigen == documento && $0.destino == destino
            }
            return guardar(archivo)
        }
    }

    private func cargar() -> ArchivoBacklinksIPad {
        guard let datos = try? Data(contentsOf: url) else {
            return ArchivoBacklinksIPad()
        }
        let iso = JSONDecoder()
        iso.dateDecodingStrategy = .iso8601
        if let valor = try? iso.decode(ArchivoBacklinksIPad.self, from: datos) {
            return ArchivoBacklinksIPad(version: valor.version,
                                        backlinks: deduplicar(valor.backlinks))
        }
        if let legado = try? JSONDecoder().decode(ArchivoBacklinksIPad.self, from: datos) {
            return ArchivoBacklinksIPad(version: legado.version,
                                        backlinks: deduplicar(legado.backlinks))
        }
        return ArchivoBacklinksIPad()
    }

    private func deduplicar(_ backlinks: [BacklinkCuadernUIPad])
        -> [BacklinkCuadernUIPad] {
        var posiciones: [ClaveSemanticaBacklinkIPad: Int] = [:]
        var resultado: [BacklinkCuadernUIPad] = []
        resultado.reserveCapacity(backlinks.count)
        for backlink in backlinks {
            let clave = backlink.claveSemantica
            if let indice = posiciones[clave] {
                let anterior = resultado[indice]
                var fusionado = backlink
                fusionado.id = anterior.id
                fusionado.fecha = min(anterior.fecha, backlink.fecha)
                resultado[indice] = fusionado
            } else {
                posiciones[clave] = resultado.count
                resultado.append(backlink)
            }
        }
        return resultado
    }

    private func guardar(_ archivo: ArchivoBacklinksIPad) -> Bool {
        do {
            let encoder = JSONEncoder()
            encoder.dateEncodingStrategy = .iso8601
            encoder.outputFormatting = [.sortedKeys]
            let datos = try encoder.encode(archivo)
            try datos.write(to: url, options: [.atomic, .completeFileProtection])
            return true
        } catch {
            return false
        }
    }
}
