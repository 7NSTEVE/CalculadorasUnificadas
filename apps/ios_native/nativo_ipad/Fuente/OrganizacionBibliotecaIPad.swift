import Foundation
import UIKit

struct CarpetaBibliotecaIPad: Codable, Equatable, Identifiable {
    var id: UUID = UUID()
    var nombre: String
    var padreId: UUID?
    var colorARGB: UInt32 = 0xFF5E5CE6
    var simboloSF: String = "folder.fill"
    var orden: Int = 0
    var creada = Date()
    var actualizada = Date()
}

struct MetadatosCuadernoOrganizadoIPad: Codable, Equatable, Identifiable {
    var id: String { cuadernoId }
    var cuadernoId: String
    var carpetaId: UUID?
    var favorito = false
    var fijado = false
    var actualizado = Date()
}

struct InstantaneaOrganizacionBibliotecaIPad {
    var carpetas: [CarpetaBibliotecaIPad]
    var cuadernos: [MetadatosCuadernoOrganizadoIPad]
}

private struct ArchivoOrganizacionBibliotecaIPad: Codable {
    var version = 1
    var carpetas: [CarpetaBibliotecaIPad] = []
    var cuadernos: [MetadatosCuadernoOrganizadoIPad] = []
}

enum ErrorOrganizacionBibliotecaIPad: LocalizedError, Equatable {
    case nombreVacio
    case carpetaInexistente
    case jerarquiaCircular
    case carpetaNoVacia

    var errorDescription: String? {
        switch self {
        case .nombreVacio: return "La carpeta necesita un nombre."
        case .carpetaInexistente: return "La carpeta seleccionada ya no existe."
        case .jerarquiaCircular: return "Una carpeta no puede contenerse a sí misma ni a uno de sus padres."
        case .carpetaNoVacia: return "Mueva primero los cuadernos y subcarpetas antes de eliminarla."
        }
    }
}

final class RepositorioOrganizacionBibliotecaIPad {
    static let compartido = RepositorioOrganizacionBibliotecaIPad()
    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var estado: ArchivoOrganizacionBibliotecaIPad

    init(base: URL? = nil, archivos: FileManager = .default) {
        let carpeta = base ?? archivos.urls(for: .applicationSupportDirectory,
                                             in: .userDomainMask)[0]
            .appendingPathComponent("Biblioteca/Organizacion", isDirectory: true)
        try? archivos.createDirectory(at: carpeta, withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("organizacion-v1.json")
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url),
           let leido = try? decoder.decode(ArchivoOrganizacionBibliotecaIPad.self, from: datos) {
            estado = leido
        } else { estado = ArchivoOrganizacionBibliotecaIPad() }
    }

    func instantanea() -> InstantaneaOrganizacionBibliotecaIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return .init(carpetas: estado.carpetas.sorted(ordenCarpetas), cuadernos: estado.cuadernos)
    }

    func carpetas(en padre: UUID?) -> [CarpetaBibliotecaIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return estado.carpetas.filter { $0.padreId == padre }.sorted(by: ordenCarpetas)
    }

    func metadata(cuadernoId: String) -> MetadatosCuadernoOrganizadoIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return estado.cuadernos.first { $0.cuadernoId == cuadernoId }
            ?? MetadatosCuadernoOrganizadoIPad(cuadernoId: cuadernoId)
    }

    @discardableResult
    func crearCarpeta(nombre: String, padreId: UUID? = nil,
                      colorARGB: UInt32 = 0xFF5E5CE6,
                      simboloSF: String = "folder.fill") throws -> CarpetaBibliotecaIPad {
        let nombre = nombre.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !nombre.isEmpty else { throw ErrorOrganizacionBibliotecaIPad.nombreVacio }
        bloqueo.lock(); defer { bloqueo.unlock() }
        if let padreId, !estado.carpetas.contains(where: { $0.id == padreId }) {
            throw ErrorOrganizacionBibliotecaIPad.carpetaInexistente
        }
        let orden = (estado.carpetas.filter { $0.padreId == padreId }.map(\.orden).max() ?? -1) + 1
        let carpeta = CarpetaBibliotecaIPad(nombre: nombre, padreId: padreId,
                                            colorARGB: colorARGB,
                                            simboloSF: Self.simboloSeguro(simboloSF), orden: orden)
        estado.carpetas.append(carpeta); guardarSinBloqueo(); return carpeta
    }

    func editarCarpeta(id: UUID, nombre: String? = nil,
                       colorARGB: UInt32? = nil, simboloSF: String? = nil) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard let i = estado.carpetas.firstIndex(where: { $0.id == id }) else {
            throw ErrorOrganizacionBibliotecaIPad.carpetaInexistente
        }
        if let nombre {
            let limpio = nombre.trimmingCharacters(in: .whitespacesAndNewlines)
            guard !limpio.isEmpty else { throw ErrorOrganizacionBibliotecaIPad.nombreVacio }
            estado.carpetas[i].nombre = limpio
        }
        if let colorARGB { estado.carpetas[i].colorARGB = colorARGB }
        if let simboloSF { estado.carpetas[i].simboloSF = Self.simboloSeguro(simboloSF) }
        estado.carpetas[i].actualizada = Date(); guardarSinBloqueo()
    }

    func moverCarpeta(id: UUID, a nuevoPadre: UUID?) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard let i = estado.carpetas.firstIndex(where: { $0.id == id }) else {
            throw ErrorOrganizacionBibliotecaIPad.carpetaInexistente
        }
        if let nuevoPadre {
            guard estado.carpetas.contains(where: { $0.id == nuevoPadre }) else {
                throw ErrorOrganizacionBibliotecaIPad.carpetaInexistente
            }
            guard nuevoPadre != id, !descendientesSinBloqueo(de: id).contains(nuevoPadre) else {
                throw ErrorOrganizacionBibliotecaIPad.jerarquiaCircular
            }
        }
        estado.carpetas[i].padreId = nuevoPadre
        estado.carpetas[i].orden = (estado.carpetas.filter { $0.padreId == nuevoPadre }.map(\.orden).max() ?? -1) + 1
        estado.carpetas[i].actualizada = Date(); guardarSinBloqueo()
    }

    func moverCuaderno(_ cuadernoId: String, a carpetaId: UUID?) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        if let carpetaId, !estado.carpetas.contains(where: { $0.id == carpetaId }) {
            throw ErrorOrganizacionBibliotecaIPad.carpetaInexistente
        }
        mutarMetadataSinBloqueo(cuadernoId) { $0.carpetaId = carpetaId }
        guardarSinBloqueo()
    }

    func favorito(_ cuadernoId: String, _ valor: Bool) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        mutarMetadataSinBloqueo(cuadernoId) { $0.favorito = valor }; guardarSinBloqueo()
    }

    func fijado(_ cuadernoId: String, _ valor: Bool) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        mutarMetadataSinBloqueo(cuadernoId) { $0.fijado = valor }; guardarSinBloqueo()
    }

    func eliminarCarpeta(_ id: UUID) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard estado.carpetas.contains(where: { $0.id == id }) else { return }
        guard !estado.carpetas.contains(where: { $0.padreId == id }),
              !estado.cuadernos.contains(where: { $0.carpetaId == id }) else {
            throw ErrorOrganizacionBibliotecaIPad.carpetaNoVacia
        }
        estado.carpetas.removeAll { $0.id == id }; guardarSinBloqueo()
    }

    func limpiarCuadernoEliminado(_ cuadernoId: String) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        estado.cuadernos.removeAll { $0.cuadernoId == cuadernoId }; guardarSinBloqueo()
    }

    func ruta(carpetaId: UUID?) -> [CarpetaBibliotecaIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard var actual = carpetaId else { return [] }
        var ruta: [CarpetaBibliotecaIPad] = [], vistos = Set<UUID>()
        while let carpeta = estado.carpetas.first(where: { $0.id == actual }), vistos.insert(actual).inserted {
            ruta.append(carpeta)
            guard let padre = carpeta.padreId else { break }
            actual = padre
        }
        return ruta.reversed()
    }

    private func mutarMetadataSinBloqueo(_ id: String,
                                         _ cambios: (inout MetadatosCuadernoOrganizadoIPad) -> Void) {
        if let i = estado.cuadernos.firstIndex(where: { $0.cuadernoId == id }) {
            cambios(&estado.cuadernos[i]); estado.cuadernos[i].actualizado = Date()
        } else {
            var nuevo = MetadatosCuadernoOrganizadoIPad(cuadernoId: id)
            cambios(&nuevo); nuevo.actualizado = Date(); estado.cuadernos.append(nuevo)
        }
    }

    private func descendientesSinBloqueo(de id: UUID) -> Set<UUID> {
        var resultado = Set<UUID>(), frontera = [id]
        while let padre = frontera.popLast() {
            for hijo in estado.carpetas where hijo.padreId == padre && resultado.insert(hijo.id).inserted {
                frontera.append(hijo.id)
            }
        }
        return resultado
    }

    private func guardarSinBloqueo() {
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        if let datos = try? encoder.encode(estado) {
            try? datos.write(to: url, options: [.atomic, .completeFileProtection])
        }
    }

    private func ordenCarpetas(_ a: CarpetaBibliotecaIPad, _ b: CarpetaBibliotecaIPad) -> Bool {
        if a.orden != b.orden { return a.orden < b.orden }
        return a.nombre.localizedCaseInsensitiveCompare(b.nombre) == .orderedAscending
    }

    private static func simboloSeguro(_ valor: String) -> String {
        let permitidos = ["folder.fill", "folder.badge.gearshape", "graduationcap.fill", "book.closed.fill",
                          "function", "resistor", "waveform.path.ecg", "atom", "hammer.fill", "star.fill"]
        return permitidos.contains(valor) ? valor : "folder.fill"
    }
}

struct MarcadorDocumentoIPad: Codable, Equatable, Identifiable {
    var id: UUID = UUID()
    var documentoId: DocumentoPersistenteID
    var pagina: Int?
    var frameId: UUID?
    var titulo: String
    var nota: String = ""
    var colorARGB: UInt32 = 0xFFFF9F0A
    var creado = Date()
    var actualizado = Date()
}

private struct ArchivoMarcadoresDocumentoIPad: Codable {
    var version = 1
    var marcadores: [MarcadorDocumentoIPad] = []
}

final class RepositorioMarcadoresDocumentoIPad {
    static let compartido = RepositorioMarcadoresDocumentoIPad()
    private let base: URL; private let bloqueo = NSLock()

    init(base: URL? = nil, archivos: FileManager = .default) {
        self.base = base ?? archivos.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Biblioteca/Marcadores", isDirectory: true)
        try? archivos.createDirectory(at: self.base, withIntermediateDirectories: true)
    }

    func marcadores(documentoId: DocumentoPersistenteID) -> [MarcadorDocumentoIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return cargarSinBloqueo(documentoId).marcadores.sorted { $0.actualizado > $1.actualizado }
    }

    @discardableResult
    func guardar(_ marcador: MarcadorDocumentoIPad) -> Bool {
        bloqueo.lock(); defer { bloqueo.unlock() }
        var archivo = cargarSinBloqueo(marcador.documentoId), item = marcador
        item.titulo = item.titulo.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !item.titulo.isEmpty else { return false }
        item.pagina = item.pagina.map { max($0, 0) }; item.actualizado = Date()
        if let i = archivo.marcadores.firstIndex(where: { $0.id == item.id }) { archivo.marcadores[i] = item }
        else { archivo.marcadores.append(item) }
        return persistirSinBloqueo(archivo, documentoId: item.documentoId)
    }

    func eliminar(_ id: UUID, documentoId: DocumentoPersistenteID) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        var archivo = cargarSinBloqueo(documentoId); archivo.marcadores.removeAll { $0.id == id }
        _ = persistirSinBloqueo(archivo, documentoId: documentoId)
    }

    private func url(_ id: DocumentoPersistenteID) -> URL { base.appendingPathComponent("\(id.description).json") }
    private func cargarSinBloqueo(_ id: DocumentoPersistenteID) -> ArchivoMarcadoresDocumentoIPad {
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        return (try? Data(contentsOf: url(id))).flatMap { try? decoder.decode(ArchivoMarcadoresDocumentoIPad.self, from: $0) }
            ?? ArchivoMarcadoresDocumentoIPad()
    }
    private func persistirSinBloqueo(_ archivo: ArchivoMarcadoresDocumentoIPad,
                                     documentoId: DocumentoPersistenteID) -> Bool {
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601; encoder.outputFormatting = [.sortedKeys]
        guard let datos = try? encoder.encode(archivo) else { return false }
        do { try datos.write(to: url(documentoId), options: [.atomic, .completeFileProtection]); return true }
        catch { return false }
    }
}
