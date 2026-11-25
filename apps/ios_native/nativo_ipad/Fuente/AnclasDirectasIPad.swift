import Foundation
import UIKit

enum CodecAnclaRegionIPad {
    private static let prefijo = "r:"
    private static let locale = Locale(identifier: "en_US_POSIX")

    static func codificar(_ region: CGRect) -> String? {
        guard [region.minX, region.minY, region.width, region.height]
            .allSatisfy(\.isFinite),
              region.width > 0, region.height > 0 else { return nil }
        let valores = [region.minX, region.minY, region.width, region.height]
            .map { String(format: "%.3f", locale: locale, Double($0)) }
            .joined(separator: ",")
        return prefijo + valores
    }

    static func decodificar(_ valor: String) -> CGRect? {
        guard valor.hasPrefix(prefijo) else { return nil }
        let cuerpo = valor.dropFirst(prefijo.count)
        let partes = cuerpo.split(separator: ",", omittingEmptySubsequences: false)
        guard partes.count == 4 else { return nil }
        let numeros = partes.compactMap { Double(String($0)) }
        guard numeros.count == 4, numeros.allSatisfy(\.isFinite),
              numeros[2] > 0, numeros[3] > 0 else { return nil }
        return CGRect(
            x: numeros[0], y: numeros[1],
            width: numeros[2], height: numeros[3])
    }
}

struct AnclaDirectaIPad: Codable, Equatable {
    var fuente: ReferenciaFuente
    var claves: [String]
    var region: CGRect
    var creada = Date()

    var esRegionFija: Bool {
        fuente.elementoId.flatMap(CodecAnclaRegionIPad.decodificar) != nil
    }
}

/// Portapapeles local de anclas. Los destinos ligados a elementos usan las
/// claves estables t:/x:/m:. Una región deliberadamente fija usa `r:x,y,w,h`;
/// así queda claro que esa ancla sigue coordenadas y no un objeto móvil.
final class PortapapelesAnclasDirectasIPad {
    static let compartido = PortapapelesAnclasDirectasIPad()

    private let bloqueo = NSLock()
    private var valor: AnclaDirectaIPad?
    private init() {}

    var ancla: AnclaDirectaIPad? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return valor
    }

    @discardableResult
    func copiar(
        seleccion: SeleccionLienzo,
        sesion: Sesion,
        titulo: String,
        cursoId: UUID? = nil
    ) -> AnclaDirectaIPad? {
        let claves = RepositorioMetadatosSeleccionIPad.compartido
            .claves(de: seleccion, sesion: sesion)
        guard !claves.isEmpty,
              let region = ExportadorSeleccionIPad.marco(seleccion, sesion: sesion),
              region.width.isFinite, region.height.isFinite,
              region.width > 0, region.height > 0 else { return nil }
        let limpio = titulo.trimmingCharacters(in: .whitespacesAndNewlines)
        let fuente = ReferenciaFuente(
            tipo: .apuntePropio,
            titulo: limpio.isEmpty ? "Ancla de CuadernU" : limpio,
            cursoId: cursoId,
            documentoId: sesion.documentoPersistenteId,
            elementoId: elegirClavePrimaria(claves, sesion: sesion, region: region))
        let nueva = AnclaDirectaIPad(
            fuente: fuente, claves: claves, region: region)
        bloqueo.lock(); valor = nueva; bloqueo.unlock()
        return nueva
    }

    @discardableResult
    func copiarRegion(
        _ region: CGRect,
        sesion: Sesion,
        titulo: String = "Región del lienzo",
        cursoId: UUID? = nil
    ) -> AnclaDirectaIPad? {
        guard let claveRegion = CodecAnclaRegionIPad.codificar(region) else {
            return nil
        }
        let limpio = titulo.trimmingCharacters(in: .whitespacesAndNewlines)
        let fuente = ReferenciaFuente(
            tipo: .apuntePropio,
            titulo: limpio.isEmpty ? "Región del lienzo" : limpio,
            cursoId: cursoId,
            documentoId: sesion.documentoPersistenteId,
            elementoId: claveRegion)
        let nueva = AnclaDirectaIPad(
            fuente: fuente, claves: [], region: region)
        bloqueo.lock(); valor = nueva; bloqueo.unlock()
        return nueva
    }

    func limpiar() {
        bloqueo.lock(); valor = nil; bloqueo.unlock()
    }

    private func elegirClavePrimaria(
        _ claves: [String],
        sesion: Sesion,
        region: CGRect
    ) -> String {
        let centro = CGPoint(x: region.midX, y: region.midY)
        let candidatos: [(String, CGRect)] = claves.compactMap { clave in
            if clave.hasPrefix("t:") {
                let id = String(clave.dropFirst(2))
                guard let indice = sesion.indiceTrazo(id: id),
                      let marco = sesion.marcoTrazo(indice) else { return nil }
                return (clave, marco)
            }
            if clave.hasPrefix("x:") {
                let id = String(clave.dropFirst(2))
                return sesion.textos.first(where: { $0.id == id }).map { (clave, $0.marco) }
            }
            if clave.hasPrefix("m:") {
                let id = String(clave.dropFirst(2))
                return sesion.medios.first(where: { $0.id == id }).map { (clave, $0.marco) }
            }
            return nil
        }
        return candidatos.min {
            distancia2(centro, $0.1) < distancia2(centro, $1.1)
        }?.0 ?? claves[0]
    }

    private func distancia2(_ punto: CGPoint, _ rect: CGRect) -> CGFloat {
        let dx = punto.x - rect.midX
        let dy = punto.y - rect.midY
        return dx * dx + dy * dy
    }
}

enum EnlazadorDirectoIPad {
    @discardableResult
    static func vincular(
        seleccionOrigen: SeleccionLienzo,
        sesion: Sesion,
        tituloOrigen: String,
        ancla: AnclaDirectaIPad
    ) -> ReferenciaCruzadaIPad? {
        let clavesOrigen = RepositorioMetadatosSeleccionIPad.compartido
            .claves(de: seleccionOrigen, sesion: sesion)
        guard !clavesOrigen.isEmpty else { return nil }
        if ancla.fuente.documentoId == sesion.documentoPersistenteId,
           !ancla.claves.isEmpty,
           !Set(clavesOrigen).isDisjoint(with: Set(ancla.claves)) {
            return nil
        }
        return RepositorioSemanticaIPad.compartido.vincular(
            documento: sesion.documentoPersistenteId,
            claves: clavesOrigen,
            titulo: tituloOrigen,
            destino: ancla.fuente)
    }
}
