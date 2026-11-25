import CoreGraphics
import Foundation

/// Geometría pura para aislar una hoja alrededor de la zona donde se está
/// trabajando. Si el usuario ya creó un frame imprimible que contiene el centro
/// visible, ese frame gana: representa mejor la página real que cualquier
/// suposición. Sin frame, usa una hoja A4 lógica a 150 ppp y conserva la
/// orientación actual del viewport.
enum GeometriaFocoPaginaActualIPad {
    static let a4Vertical = CGSize(width: 1240, height: 1754)
    static let a4Horizontal = CGSize(width: 1754, height: 1240)

    static func region(
        visible: CGRect,
        marcosPagina: [CGRect] = []
    ) -> CGRect {
        let visibleSeguro = normalizar(visible)
        let centro = CGPoint(x: visibleSeguro.midX, y: visibleSeguro.midY)

        let candidatas = marcosPagina
            .map(normalizar)
            .filter { !$0.isEmpty && $0.contains(centro) }
            .sorted { area($0) < area($1) }
        if let exacta = candidatas.first { return exacta }

        let horizontal = visibleSeguro.width > visibleSeguro.height * 1.15
        let tamano = horizontal ? a4Horizontal : a4Vertical
        return CGRect(
            x: centro.x - tamano.width / 2,
            y: centro.y - tamano.height / 2,
            width: tamano.width,
            height: tamano.height)
    }

    /// Región que se envía al zoom. Es deliberadamente un poco mayor que la
    /// hoja para que el borde siga siendo visible y la máscara tenga un exterior
    /// que oscurecer, en vez de hacer que la página ocupe cada píxel de pantalla.
    static func contextoVisual(
        para pagina: CGRect,
        margenRelativo: CGFloat = 0.06
    ) -> CGRect {
        let segura = normalizar(pagina)
        guard !segura.isEmpty else { return segura }
        let margen = min(max(margenRelativo, 0), 0.5)
        return segura.insetBy(
            dx: -segura.width * margen,
            dy: -segura.height * margen)
    }

    private static func normalizar(_ rect: CGRect) -> CGRect {
        guard rect.origin.x.isFinite, rect.origin.y.isFinite,
              rect.width.isFinite, rect.height.isFinite else { return .zero }
        return rect.standardized
    }

    private static func area(_ rect: CGRect) -> CGFloat {
        max(rect.width, 0) * max(rect.height, 0)
    }
}

struct EventoFocoPaginaActualIPad: Equatable {
    var documento: UInt64
    /// `nil` significa quitar la máscara.
    var region: CGRect?
    /// Texto efímero que puede acompañar una práctica (por ejemplo, el
    /// enunciado de un problema). Nunca se persiste dentro de la tinta.
    var guia: String? = nil
}

/// Canal ligero entre el panel de navegación y el contenedor del lienzo. No
/// persiste estado: una máscara de concentración es estado de vista, no del
/// documento. El documento filtra el evento y la región permite distinguir el
/// viewport que originó la acción cuando el mismo cuaderno aparece dos veces.
enum CanalFocoPaginaActualIPad {
    static let nombre = Notification.Name("CuadernU.focoPaginaActual")
    private static let claveDocumento = "documento"
    private static let claveRegion = "region"
    private static let claveGuia = "guia"

    static func activar(
        documento: UInt64,
        region: CGRect,
        guia: String? = nil,
        centro: NotificationCenter = .default
    ) {
        var informacion: [AnyHashable: Any] = [
            claveDocumento: NSNumber(value: documento),
            claveRegion: NSValue(cgRect: region),
        ]
        if let guia = guia?.trimmingCharacters(in: .whitespacesAndNewlines),
           !guia.isEmpty {
            informacion[claveGuia] = guia
        }
        centro.post(name: nombre, object: nil, userInfo: informacion)
    }

    /// Cuando una orden primero mueve el viewport hacia un frame lejano, el
    /// receptor todavía puede estar mostrando la región anterior en el primer
    /// ciclo de run loop. Reemitir de forma acotada permite que el viewport
    /// correcto acepte la máscara en cuanto el centro del frame sea visible,
    /// sin convertir el foco en estado persistente ni usar polling continuo.
    static func activarCuandoSeaVisible(
        documento: UInt64,
        region: CGRect,
        guia: String? = nil,
        demoras: [TimeInterval] = [0, 0.18, 0.45],
        centro: NotificationCenter = .default
    ) {
        let demorasValidas = demoras.filter { $0 >= 0 }
        for demora in demorasValidas {
            DispatchQueue.main.asyncAfter(deadline: .now() + demora) {
                activar(
                    documento: documento,
                    region: region,
                    guia: guia,
                    centro: centro)
            }
        }
        if centro === NotificationCenter.default {
            let ultima = demorasValidas.max() ?? 0
            ActualizadorHUDSimulacroIPad.compartido.programarSiCorresponde(
                documento: documento,
                region: region,
                demora: ultima + 0.05)
        }
    }

    static func desactivar(
        documento: UInt64,
        centro: NotificationCenter = .default
    ) {
        if centro === NotificationCenter.default {
            ActualizadorHUDSimulacroIPad.compartido.detener(documento: documento)
        }
        centro.post(
            name: nombre,
            object: nil,
            userInfo: [claveDocumento: NSNumber(value: documento)])
    }

    static func evento(de notificacion: Notification) -> EventoFocoPaginaActualIPad? {
        guard notificacion.name == nombre,
              let numero = notificacion.userInfo?[claveDocumento] as? NSNumber else {
            return nil
        }
        let region = (notificacion.userInfo?[claveRegion] as? NSValue)?.cgRectValue
        let guia = notificacion.userInfo?[claveGuia] as? String
        return EventoFocoPaginaActualIPad(
            documento: numero.uint64Value,
            region: region,
            guia: guia)
    }
}
