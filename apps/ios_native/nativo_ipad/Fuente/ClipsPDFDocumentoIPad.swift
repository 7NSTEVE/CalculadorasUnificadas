import Foundation
import UIKit

/// Sidecar documental de clips PDF.
///
/// El repositorio histórico de procedencia guarda los clips únicamente por
/// `elementoId`. Como una copia de documento conserva los IDs internos de sus
/// medios, esa clave global hacía que original y duplicado compartieran la
/// máscara libre/poligonal. La procedencia del archivo fuente por hash sí puede
/// seguir siendo global; únicamente la geometría mutable del clip se separa por
/// identidad durable de documento.
final class RepositorioClipsPDFDocumentoIPad {
    static let compartido = RepositorioClipsPDFDocumentoIPad()

    private struct Archivo: Codable {
        var formato = 1
        var porDocumento: [String: [String: ClipConFuenteIPad]] = [:]
    }

    private let cola = DispatchQueue(label: "tinta.clips-pdf.documento")
    private let url: URL
    private var archivo: Archivo

    private init() {
        let soporte = FileManager.default.urls(for: .applicationSupportDirectory,
                                               in: .userDomainMask)[0]
        let carpeta = soporte.appendingPathComponent("Tinta/Procedencia", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("clips-documento.json")
        archivo = (try? Data(contentsOf: url)).flatMap {
            try? JSONDecoder().decode(Archivo.self, from: $0)
        } ?? Archivo()
    }

    private func clave(_ documento: UInt64) -> String { String(documento) }

    /// Lee la máscara efectiva. Una entrada legacy se materializa dentro del
    /// documento en la primera lectura para que copias posteriores puedan
    /// divergir sin modificar el original.
    func clip(documento: UInt64, elementoId: String) -> ClipConFuenteIPad? {
        cola.sync {
            let documentoClave = clave(documento)
            if let valor = archivo.porDocumento[documentoClave]?[elementoId] {
                return valor
            }
            guard let heredado = RepositorioProcedenciaIPad.compartido
                .clip(elementoId: elementoId) else { return nil }
            var siguiente = archivo
            var clips = siguiente.porDocumento[documentoClave] ?? [:]
            clips[elementoId] = heredado
            siguiente.porDocumento[documentoClave] = clips
            do {
                try persistir(siguiente)
                archivo = siguiente
            } catch {
                // Puede mostrarse el clip legacy aun si falla la migración. Una
                // escritura explícita sí reporta el fallo mediante `false`.
            }
            return heredado
        }
    }

    @discardableResult
    func registrar(_ clip: ClipConFuenteIPad, documento: UInt64) -> Bool {
        cola.sync {
            let documentoClave = clave(documento)
            var siguiente = archivo
            var clips = siguiente.porDocumento[documentoClave] ?? [:]
            clips[clip.elementoId] = clip
            siguiente.porDocumento[documentoClave] = clips
            do {
                try persistir(siguiente)
                archivo = siguiente
                return true
            } catch {
                return false
            }
        }
    }

    /// Duplica una máscara al crear otro elemento dentro del mismo documento.
    @discardableResult
    func duplicarElemento(documento: UInt64,
                          origen: String,
                          destino: String) -> Bool {
        guard origen != destino,
              var valor = clip(documento: documento, elementoId: origen) else {
            return false
        }
        valor.id = UUID()
        valor.elementoId = destino
        valor.fecha = Date()
        return registrar(valor, documento: documento)
    }

    /// Copia las máscaras ya materializadas de un documento completo. Los IDs
    /// de medio se conservan porque también se conservan dentro del blob `.tinta`;
    /// el namespace del documento es el que cambia.
    @discardableResult
    func duplicar(desde origen: UInt64, hacia destino: UInt64) throws -> Int {
        guard origen != destino else { throw CocoaError(.fileWriteInvalidFileName) }
        return try cola.sync {
            let origenClave = clave(origen)
            let destinoClave = clave(destino)
            let existente = archivo.porDocumento[destinoClave] ?? [:]
            guard existente.isEmpty else { throw CocoaError(.fileWriteFileExists) }

            let fuente = archivo.porDocumento[origenClave] ?? [:]
            var copia: [String: ClipConFuenteIPad] = [:]
            copia.reserveCapacity(fuente.count)
            for (elementoId, var clip) in fuente {
                clip.id = UUID()
                clip.elementoId = elementoId
                clip.fecha = Date()
                copia[elementoId] = clip
            }

            var siguiente = archivo
            siguiente.porDocumento[destinoClave] = copia
            try persistir(siguiente)
            archivo = siguiente
            return copia.count
        }
    }

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

    func instantanea(documento: UInt64) -> [String: ClipConFuenteIPad] {
        cola.sync { archivo.porDocumento[clave(documento)] ?? [:] }
    }

    private func persistir(_ valor: Archivo) throws {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.sortedKeys]
        let datos = try encoder.encode(valor)
        try datos.write(to: url, options: [.atomic, .completeFileProtection])
    }
}

/// Renderiza únicamente la geometría documental del clip. La fuente del PDF
/// continúa resolviéndose por hash en `RepositorioProcedenciaIPad`.
enum RenderizadorClipPDFDocumentoIPad {
    static func ruta(documento: UInt64,
                     elementoId: String,
                     en rect: CGRect) -> UIBezierPath? {
        guard let clip = RepositorioClipsPDFDocumentoIPad.compartido.clip(
            documento: documento, elementoId: elementoId),
              clip.forma == .libre || clip.forma == .poligonal,
              let puntos = clip.puntosNormalizados, puntos.count >= 3,
              clip.regionNormalizada.width > 0,
              clip.regionNormalizada.height > 0 else { return nil }

        let ruta = UIBezierPath()
        for (indice, punto) in puntos.enumerated() {
            let local = CGPoint(
                x: rect.minX + (punto.x - clip.regionNormalizada.minX)
                    / clip.regionNormalizada.width * rect.width,
                y: rect.minY + (punto.y - clip.regionNormalizada.minY)
                    / clip.regionNormalizada.height * rect.height)
            if indice == 0 { ruta.move(to: local) }
            else { ruta.addLine(to: local) }
        }
        ruta.close()
        return ruta
    }

    static func enmascarar(_ imagen: UIImage,
                           documento: UInt64,
                           elementoId: String) -> UIImage {
        let rect = CGRect(origin: .zero, size: imagen.size)
        guard ruta(documento: documento, elementoId: elementoId, en: rect) != nil else {
            return imagen
        }
        let formato = UIGraphicsImageRendererFormat()
        formato.opaque = false
        formato.scale = imagen.scale
        return UIGraphicsImageRenderer(size: imagen.size, format: formato).image { _ in
            ruta(documento: documento, elementoId: elementoId, en: rect)?.addClip()
            imagen.draw(at: .zero)
        }
    }
}
