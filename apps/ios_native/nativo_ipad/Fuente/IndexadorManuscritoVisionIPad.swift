import Foundation
import UIKit
import Vision

struct RegionOCRManuscritoPlanIPad: Equatable, Identifiable {
    var id: String { fuenteId }
    var fuenteId: String
    var documentoId: UInt64
    var indicesTrazos: [Int]
    var idsTrazos: [String]
    var marco: CGRect
}

private struct TrazoSnapshotOCRIPad {
    var triangulos: [CGPoint]
}

private struct RegionSnapshotOCRIPad {
    var plan: RegionOCRManuscritoPlanIPad
    var trazos: [TrazoSnapshotOCRIPad]
}

private struct ResultadoVisionOCRIPad {
    var texto: String
    var confianza: Float
    var idiomas: [String]
}

private struct ConfiguracionVisionOCRIPad {
    var idiomas: [String]
    var firmaMotor: String
}

enum PlanificadorOCRManuscritoIPad {
    static let maximoTrazosRegion = 96
    static let maximoAnchoRegion: CGFloat = 1_900
    static let maximoAltoRegion: CGFloat = 320

    static func regiones(sesion: Sesion) -> [RegionOCRManuscritoPlanIPad] {
        struct Item {
            var indice: Int
            var id: String
            var marco: CGRect
        }
        let documento = sesion.documentoPersistenteId
        var items: [Item] = []
        items.reserveCapacity(sesion.cantidadTrazos)
        for indice in 0..<sesion.cantidadTrazos {
            guard let id = sesion.idTrazo(indice), !id.isEmpty,
                  let marco = sesion.marcoTrazo(indice),
                  marco.minX.isFinite, marco.minY.isFinite,
                  marco.width.isFinite, marco.height.isFinite,
                  marco.width > 0.2, marco.height > 0.2 else { continue }
            items.append(.init(indice: indice, id: id, marco: marco))
        }
        items.sort {
            if abs($0.marco.midY - $1.marco.midY) > 4 {
                return $0.marco.midY < $1.marco.midY
            }
            return $0.marco.minX < $1.marco.minX
        }

        var grupos: [[Item]] = []
        var actual: [Item] = []
        var marcoActual = CGRect.null

        func cerrarActual() {
            guard !actual.isEmpty else { return }
            grupos.append(actual)
            actual.removeAll(keepingCapacity: true)
            marcoActual = .null
        }

        for item in items {
            guard !actual.isEmpty else {
                actual = [item]
                marcoActual = item.marco
                continue
            }
            let alturaReferencia = max(marcoActual.height, item.marco.height, 12)
            let toleranciaVertical = min(max(alturaReferencia * 0.75, 16), 54)
            let solapaVertical = item.marco.minY <= marcoActual.maxY + toleranciaVertical
                && item.marco.maxY >= marcoActual.minY - toleranciaVertical
            let huecoHorizontal = item.marco.minX > marcoActual.maxX
                ? item.marco.minX - marcoActual.maxX
                : (marcoActual.minX > item.marco.maxX
                    ? marcoActual.minX - item.marco.maxX : 0)
            let candidato = marcoActual.union(item.marco)
            let cabe = actual.count < maximoTrazosRegion
                && candidato.width <= maximoAnchoRegion
                && candidato.height <= maximoAltoRegion
            if solapaVertical && huecoHorizontal <= 180 && cabe {
                actual.append(item)
                marcoActual = candidato
            } else {
                cerrarActual()
                actual = [item]
                marcoActual = item.marco
            }
        }
        cerrarActual()

        return grupos.compactMap { grupo in
            guard let marco = grupo.map(\.marco).reduce(
                nil as CGRect?, { $0?.union($1) ?? $1 }) else { return nil }
            let ids = grupo.map(\.id).sorted()
            return RegionOCRManuscritoPlanIPad(
                fuenteId: RepositorioOCRManuscritoIPad.fuenteId(
                    documentoId: documento, idsTrazos: ids),
                documentoId: documento,
                indicesTrazos: grupo.map(\.indice),
                idsTrazos: ids,
                marco: marco.insetBy(dx: -10, dy: -8))
        }
    }
}

@MainActor
final class IndexadorManuscritoVisionIPad {
    static let compartido = IndexadorManuscritoVisionIPad()

    private let colaVision = DispatchQueue(
        label: "cuadernu.ocr.manuscrito.vision",
        qos: .utility)
    private var generaciones: [UInt64: UUID] = [:]
    private var documentosEnCurso = Set<UInt64>()
    let maximoRegionesPorPasada = 24

    struct ResultadoPasada: Equatable {
        var documentoId: UInt64
        var regionesTotales: Int
        var regionesReutilizadas: Int
        var regionesProcesadas: Int
        var regionesPendientes: Int
        var reconocidas: Int
        var error: String?
    }

    func cancelar(documentoId: UInt64) {
        generaciones[documentoId] = UUID()
        documentosEnCurso.remove(documentoId)
    }

    func indexar(
        sesion: Sesion,
        repositorio: RepositorioOCRManuscritoIPad = .compartido,
        completado: @escaping (ResultadoPasada) -> Void
    ) {
        let documento = sesion.documentoPersistenteId
        guard !documentosEnCurso.contains(documento) else { return }
        documentosEnCurso.insert(documento)
        let generacion = UUID()
        generaciones[documento] = generacion

        let planes = PlanificadorOCRManuscritoIPad.regiones(sesion: sesion)
        let preferencias = PreferenciasOCRManuscritoIPad.actual
        let configuracion = Self.configuracionVision(preferencias)
        let antiguos = repositorio.registros(documentoId: documento)
        let antiguosPorId = Dictionary(uniqueKeysWithValues: antiguos.map {
            ($0.fuenteId, $0)
        })
        let fuentesVigentes = Set(planes.map(\.fuenteId))

        var salida: [RegistroOCRManuscritoIPad] = []
        var pendientes: [RegionOCRManuscritoPlanIPad] = []
        for plan in planes {
            if var anterior = antiguosPorId[plan.fuenteId],
               anterior.idiomas == configuracion.idiomas,
               anterior.firmaMotor == configuracion.firmaMotor {
                // Mover/reflow conserva semántica si siguen siendo exactamente
                // los mismos IDs. Actualizamos la caja sin volver a llamar Vision.
                anterior.marco = plan.marco
                anterior.idsTrazos = plan.idsTrazos
                salida.append(anterior)
            } else {
                pendientes.append(plan)
            }
        }

        let lote = Array(pendientes.prefix(maximoRegionesPorPasada))
        let resto = Array(pendientes.dropFirst(lote.count))
        // Mientras una región vieja espera re-OCR por cambio de motor/idioma, se
        // conserva su texto anterior. Las fuentes que ya no existen se eliminan.
        for plan in resto {
            if let anterior = antiguosPorId[plan.fuenteId] {
                salida.append(anterior)
            }
        }
        salida.removeAll { !fuentesVigentes.contains($0.fuenteId) }

        procesar(
            lote,
            indice: 0,
            sesion: sesion,
            preferencias: preferencias,
            configuracion: configuracion,
            generacion: generacion,
            salida: salida,
            reconocidas: 0,
            reutilizadas: planes.count - pendientes.count,
            pendientesPosteriores: resto.count,
            repositorio: repositorio,
            completado: completado)
    }

    private static func configuracionVision(
        _ preferencias: PreferenciasOCRManuscritoIPad
    ) -> ConfiguracionVisionOCRIPad {
        let solicitud = VNRecognizeTextRequest()
        solicitud.recognitionLevel = .accurate
        let idiomas = preferencias.idiomasSoportados(por: solicitud)
        let sistema = ProcessInfo.processInfo.operatingSystemVersionString
        let firma = "vision-text-r\(solicitud.revision)|\(sistema)"
        return .init(idiomas: idiomas, firmaMotor: firma)
    }

    private func procesar(
        _ lote: [RegionOCRManuscritoPlanIPad],
        indice: Int,
        sesion: Sesion,
        preferencias: PreferenciasOCRManuscritoIPad,
        configuracion: ConfiguracionVisionOCRIPad,
        generacion: UUID,
        salida: [RegistroOCRManuscritoIPad],
        reconocidas: Int,
        reutilizadas: Int,
        pendientesPosteriores: Int,
        repositorio: RepositorioOCRManuscritoIPad,
        completado: @escaping (ResultadoPasada) -> Void
    ) {
        let documento = sesion.documentoPersistenteId
        guard generaciones[documento] == generacion else {
            documentosEnCurso.remove(documento)
            return
        }
        guard indice < lote.count else {
            do {
                var unicos: [String: RegistroOCRManuscritoIPad] = [:]
                for valor in salida { unicos[valor.fuenteId] = valor }
                try repositorio.reemplazarDocumento(
                    documento, con: Array(unicos.values))
                documentosEnCurso.remove(documento)
                completado(.init(
                    documentoId: documento,
                    regionesTotales: reutilizadas + lote.count
                        + pendientesPosteriores,
                    regionesReutilizadas: reutilizadas,
                    regionesProcesadas: lote.count,
                    regionesPendientes: pendientesPosteriores,
                    reconocidas: reconocidas,
                    error: nil))
            } catch {
                documentosEnCurso.remove(documento)
                completado(.init(
                    documentoId: documento,
                    regionesTotales: reutilizadas + lote.count
                        + pendientesPosteriores,
                    regionesReutilizadas: reutilizadas,
                    regionesProcesadas: lote.count,
                    regionesPendientes: pendientesPosteriores,
                    reconocidas: reconocidas,
                    error: error.localizedDescription))
            }
            return
        }

        let plan = lote[indice]
        guard let snapshot = snapshot(plan: plan, sesion: sesion) else {
            var siguiente = salida
            siguiente.append(Self.registroNegativo(
                plan: plan, configuracion: configuracion))
            procesar(
                lote, indice: indice + 1, sesion: sesion,
                preferencias: preferencias,
                configuracion: configuracion,
                generacion: generacion,
                salida: siguiente,
                reconocidas: reconocidas,
                reutilizadas: reutilizadas,
                pendientesPosteriores: pendientesPosteriores,
                repositorio: repositorio,
                completado: completado)
            return
        }

        colaVision.async {
            let resultado = Self.reconocer(
                snapshot: snapshot,
                preferencias: preferencias,
                configuracion: configuracion)
            DispatchQueue.main.async { [weak self] in
                guard let self,
                      self.generaciones[documento] == generacion else {
                    self?.documentosEnCurso.remove(documento)
                    return
                }
                var siguiente = salida
                var cantidadReconocida = reconocidas
                if let resultado,
                   !resultado.texto.trimmingCharacters(
                    in: .whitespacesAndNewlines).isEmpty {
                    siguiente.append(.init(
                        fuenteId: plan.fuenteId,
                        documentoId: documento,
                        idsTrazos: plan.idsTrazos,
                        marco: plan.marco,
                        texto: resultado.texto,
                        confianza: resultado.confianza,
                        idiomas: resultado.idiomas,
                        firmaMotor: configuracion.firmaMotor,
                        fecha: Date()))
                    cantidadReconocida += 1
                } else {
                    siguiente.append(Self.registroNegativo(
                        plan: plan, configuracion: configuracion))
                }
                self.procesar(
                    lote, indice: indice + 1, sesion: sesion,
                    preferencias: preferencias,
                    configuracion: configuracion,
                    generacion: generacion,
                    salida: siguiente,
                    reconocidas: cantidadReconocida,
                    reutilizadas: reutilizadas,
                    pendientesPosteriores: pendientesPosteriores,
                    repositorio: repositorio,
                    completado: completado)
            }
        }
    }

    private static func registroNegativo(
        plan: RegionOCRManuscritoPlanIPad,
        configuracion: ConfiguracionVisionOCRIPad
    ) -> RegistroOCRManuscritoIPad {
        .init(
            fuenteId: plan.fuenteId,
            documentoId: plan.documentoId,
            idsTrazos: plan.idsTrazos,
            marco: plan.marco,
            texto: "",
            confianza: 0,
            idiomas: configuracion.idiomas,
            firmaMotor: configuracion.firmaMotor,
            fecha: Date())
    }

    private func snapshot(
        plan: RegionOCRManuscritoPlanIPad,
        sesion: Sesion
    ) -> RegionSnapshotOCRIPad? {
        var trazos: [TrazoSnapshotOCRIPad] = []
        trazos.reserveCapacity(plan.indicesTrazos.count)
        for indice in plan.indicesTrazos {
            guard let origen = sesion.verticesDelTrazo(indice),
                  origen.cantidad >= 3 else { continue }
            var puntos: [CGPoint] = []
            puntos.reserveCapacity(origen.cantidad)
            for i in 0..<origen.cantidad {
                puntos.append(CGPoint(
                    x: CGFloat(origen.datos[i * 2]),
                    y: CGFloat(origen.datos[i * 2 + 1])))
            }
            trazos.append(.init(triangulos: puntos))
        }
        guard !trazos.isEmpty else { return nil }
        return .init(plan: plan, trazos: trazos)
    }

    nonisolated private static func reconocer(
        snapshot: RegionSnapshotOCRIPad,
        preferencias: PreferenciasOCRManuscritoIPad,
        configuracion: ConfiguracionVisionOCRIPad
    ) -> ResultadoVisionOCRIPad? {
        guard let cg = rasterizar(snapshot) else { return nil }
        let solicitud = VNRecognizeTextRequest()
        solicitud.recognitionLevel = .accurate
        solicitud.usesLanguageCorrection = preferencias.correccionIdioma
        solicitud.automaticallyDetectsLanguage = preferencias
            .deteccionAutomaticaIdioma
        if !configuracion.idiomas.isEmpty {
            solicitud.recognitionLanguages = configuracion.idiomas
        }
        solicitud.minimumTextHeight = 0.012
        solicitud.customWords = [
            "KCL", "KVL", "Thévenin", "Norton", "Kirchhoff",
            "impedancia", "fasor", "MOSFET", "BJT", "op-amp",
            "inductancia", "capacitancia", "resonancia", "Laplace",
        ]
        do {
            try VNImageRequestHandler(cgImage: cg).perform([solicitud])
        } catch {
            return nil
        }
        let candidatos = (solicitud.results ?? []).compactMap {
            $0.topCandidates(1).first
        }.filter { $0.confidence >= 0.20 }
        guard !candidatos.isEmpty else { return nil }
        let texto = candidatos.map(\.string).joined(separator: "\n")
        let confianza = candidatos.reduce(Float(0)) { $0 + $1.confidence }
            / Float(candidatos.count)
        return .init(
            texto: texto,
            confianza: confianza,
            idiomas: configuracion.idiomas)
    }

    nonisolated private static func rasterizar(
        _ snapshot: RegionSnapshotOCRIPad
    ) -> CGImage? {
        let marco = snapshot.plan.marco
        guard marco.width > 0, marco.height > 0 else { return nil }
        let limite: CGFloat = 2_048
        let escala = min(2.0, limite / max(marco.width, marco.height, 1))
        let escalaSegura = max(escala, 0.35)
        let tamano = CGSize(
            width: max(1, ceil(marco.width * escalaSegura)),
            height: max(1, ceil(marco.height * escalaSegura)))
        guard tamano.width <= limite + 1,
              tamano.height <= limite + 1,
              tamano.width * tamano.height <= 4_194_304 else { return nil }

        let formato = UIGraphicsImageRendererFormat()
        formato.scale = 1
        formato.opaque = true
        let imagen = UIGraphicsImageRenderer(size: tamano, format: formato).image {
            contexto in
            let cg = contexto.cgContext
            cg.setFillColor(UIColor.white.cgColor)
            cg.fill(CGRect(origin: .zero, size: tamano))
            cg.saveGState()
            cg.scaleBy(x: escalaSegura, y: escalaSegura)
            cg.translateBy(x: -marco.minX, y: -marco.minY)
            cg.setFillColor(UIColor.black.cgColor)
            for trazo in snapshot.trazos {
                var i = 0
                while i + 2 < trazo.triangulos.count {
                    cg.move(to: trazo.triangulos[i])
                    cg.addLine(to: trazo.triangulos[i + 1])
                    cg.addLine(to: trazo.triangulos[i + 2])
                    cg.closePath()
                    i += 3
                }
                cg.fillPath()
            }
            cg.restoreGState()
        }
        return imagen.cgImage
    }
}
