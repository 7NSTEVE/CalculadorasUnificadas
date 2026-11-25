import CryptoKit
import Foundation
import UIKit

struct AdjuntoImportado {
    let hash: String
    let url: URL
    let nombreOriginal: String
    let bytes: Int64
}

/// Almacén local por contenido. Dos copias del mismo PDF, foto o vídeo apuntan
/// al mismo hash; el documento guarda la referencia y no duplica los bytes.
final class RepositorioAdjuntos {
    static let compartido = RepositorioAdjuntos()
    private let archivos = FileManager.default
    let carpeta: URL

    private init() {
        let documentos = archivos.urls(for: .documentDirectory,
                                        in: .userDomainMask).first!
        carpeta = documentos.appendingPathComponent("Recursos", isDirectory: true)
        try? archivos.createDirectory(at: carpeta, withIntermediateDirectories: true)
    }

    func importar(url origen: URL) throws -> AdjuntoImportado {
        let acceso = origen.startAccessingSecurityScopedResource()
        defer { if acceso { origen.stopAccessingSecurityScopedResource() } }
        let hash = try hashDeArchivo(origen)
        let extensionArchivo = origen.pathExtension.lowercased()
        let nombre = extensionArchivo.isEmpty ? hash : "\(hash).\(extensionArchivo)"
        let destino = carpeta.appendingPathComponent(nombre)
        if !archivos.fileExists(atPath: destino.path) {
            try archivos.copyItem(at: origen, to: destino)
        }
        let atributos = try archivos.attributesOfItem(atPath: destino.path)
        let bytes = (atributos[.size] as? NSNumber)?.int64Value ?? 0
        return AdjuntoImportado(hash: hash, url: destino,
                                nombreOriginal: origen.lastPathComponent, bytes: bytes)
    }

    func importar(imagen: UIImage, nombre: String = "Imagen.png") throws -> AdjuntoImportado {
        guard let datos = imagen.pngData() else { throw ErrorAdjunto.noSePudoCodificar }
        let hash = SHA256.hash(data: datos).map { String(format: "%02x", $0) }.joined()
        let destino = carpeta.appendingPathComponent("\(hash).png")
        if !archivos.fileExists(atPath: destino.path) {
            try datos.write(to: destino, options: .atomic)
        }
        return AdjuntoImportado(hash: hash, url: destino,
                                nombreOriginal: nombre, bytes: Int64(datos.count))
    }

    func url(hash: String) -> URL? {
        guard let nombres = try? archivos.contentsOfDirectory(
            at: carpeta, includingPropertiesForKeys: nil, options: [.skipsHiddenFiles]) else {
            return nil
        }
        return nombres.first { $0.deletingPathExtension().lastPathComponent == hash }
    }

    func tamanoTotal() -> Int64 {
        guard let enumerador = archivos.enumerator(
            at: carpeta, includingPropertiesForKeys: [.fileSizeKey],
            options: [.skipsHiddenFiles]) else { return 0 }
        var total: Int64 = 0
        for case let url as URL in enumerador {
            total += Int64((try? url.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0)
        }
        return total
    }

    private func hashDeArchivo(_ url: URL) throws -> String {
        let manejador = try FileHandle(forReadingFrom: url)
        defer { try? manejador.close() }
        var sha = SHA256()
        while true {
            let bloque = try manejador.read(upToCount: 1024 * 1024) ?? Data()
            if bloque.isEmpty { break }
            sha.update(data: bloque)
        }
        return sha.finalize().map { String(format: "%02x", $0) }.joined()
    }
}

enum ErrorAdjunto: LocalizedError {
    case noSePudoCodificar
    var errorDescription: String? { "No se pudo codificar el recurso." }
}
