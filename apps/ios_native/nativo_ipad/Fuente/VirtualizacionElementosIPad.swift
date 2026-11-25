import CoreGraphics
import Foundation

struct DescriptorVirtualizableIPad: Equatable, Identifiable {
    var id: String
    var marco: CGRect
    var anclado = false
}

struct PoliticaVirtualizacionElementosIPad: Equatable {
    /// Por debajo de este total la virtualización cuesta más coordinación de la
    /// que ahorra; se materializa todo como en documentos pequeños.
    var umbralElementos = 120
    /// Cuántos viewports alrededor se mantienen calientes para que un scroll
    /// rápido no muestre placeholders ni obligue a crear vistas cada frame.
    var prefetchHorizontal: CGFloat = 1.25
    var prefetchVertical: CGFloat = 2.0
    var margenMinimoHorizontal: CGFloat = 800
    var margenMinimoVertical: CGFloat = 1_200
    /// Fracción interior que actúa como histéresis. Mientras el viewport siga
    /// dentro de esta zona no se recalcula el conjunto materializado.
    var fraccionHistéresis: CGFloat = 0.22
}

struct PlanVirtualizacionElementosIPad: Equatable {
    var virtualizacionActiva: Bool
    var regionMaterializacion: CGRect?
    var idsMaterializados: Set<String>
    var reutilizaRegionAnterior: Bool
}

enum PlanificadorVirtualizacionElementosIPad {
    static func plan(
        elementos: [DescriptorVirtualizableIPad],
        viewport: CGRect,
        regionAnterior: CGRect?,
        politica: PoliticaVirtualizacionElementosIPad = .init()
    ) -> PlanVirtualizacionElementosIPad {
        let total = elementos.count
        let umbral = max(politica.umbralElementos, 1)
        guard total > umbral,
              viewport.width > 0,
              viewport.height > 0,
              !viewport.isNull,
              !viewport.isInfinite else {
            return .init(
                virtualizacionActiva: false,
                regionMaterializacion: nil,
                idsMaterializados: Set(elementos.map(\.id)),
                reutilizaRegionAnterior: false)
        }

        let margenX = max(
            viewport.width * max(politica.prefetchHorizontal, 0),
            max(politica.margenMinimoHorizontal, 0))
        let margenY = max(
            viewport.height * max(politica.prefetchVertical, 0),
            max(politica.margenMinimoVertical, 0))

        var region = viewport.insetBy(dx: -margenX, dy: -margenY)
        var reutiliza = false
        if let anterior = regionAnterior, anterior.width > 0, anterior.height > 0 {
            let f = min(max(politica.fraccionHistéresis, 0), 0.45)
            let zonaSegura = anterior.insetBy(
                dx: anterior.width * f,
                dy: anterior.height * f)
            if zonaSegura.contains(viewport) {
                region = anterior
                reutiliza = true
            }
        }

        let ids = Set(elementos.compactMap { elemento -> String? in
            if elemento.anclado { return elemento.id }
            guard !elemento.marco.isNull, !elemento.marco.isInfinite else {
                return nil
            }
            return elemento.marco.intersects(region) ? elemento.id : nil
        })
        return .init(
            virtualizacionActiva: true,
            regionMaterializacion: region,
            idsMaterializados: ids,
            reutilizaRegionAnterior: reutiliza)
    }
}
