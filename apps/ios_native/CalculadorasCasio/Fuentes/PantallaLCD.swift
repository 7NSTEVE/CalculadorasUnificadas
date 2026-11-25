import SwiftUI

/// Dibuja la matriz de puntos del LCD.
///
/// Se construye un CGImage del tamano exacto en puntos (96x31 o 192x63) y se deja
/// que lo escale la GPU con interpolacion desactivada. Pintar cada punto como un
/// rectangulo seria hasta 12.096 formas por fotograma, y a 60 Hz eso no se sostiene.
struct PantallaLCD: View {
    let pixeles: [UInt8]
    let ancho: Int
    let alto: Int

    /// Verde palido del cristal, como el de las Casio.
    private let fondo = Color(red: 0.816, green: 0.871, blue: 0.784)
    private let tinta = CGColor(red: 0.12, green: 0.16, blue: 0.29, alpha: 1)

    var body: some View {
        Canvas(rendersAsynchronously: false) { contexto, tamano in
            contexto.fill(Path(CGRect(origin: .zero, size: tamano)), with: .color(fondo))
            guard let imagen = imagenDeLaMatriz() else { return }
            contexto.withCGContext { cg in
                cg.interpolationQuality = .none
                // CGImage dibuja con el origen abajo-izquierda y este contexto lo
                // tiene arriba-izquierda: sin invertir el eje Y la pantalla sale
                // volteada y el contenido, que va arriba, aparece abajo.
                cg.translateBy(x: 0, y: tamano.height)
                cg.scaleBy(x: 1, y: -1)
                cg.draw(imagen, in: CGRect(origin: .zero, size: tamano))
            }
        }
        .drawingGroup()
    }

    private func imagenDeLaMatriz() -> CGImage? {
        guard ancho > 0, alto > 0, pixeles.count >= ancho * alto else { return nil }

        let componentes = tinta.components ?? [0, 0, 0, 1]
        let r = UInt8(max(0, min(255, componentes[0] * 255)))
        let g = UInt8(max(0, min(255, componentes[1] * 255)))
        let b = UInt8(max(0, min(255, componentes[2] * 255)))

        var rgba = [UInt8](repeating: 0, count: ancho * alto * 4)
        for indice in 0..<(ancho * alto) where pixeles[indice] != 0 {
            let destino = indice * 4
            rgba[destino] = r
            rgba[destino + 1] = g
            rgba[destino + 2] = b
            rgba[destino + 3] = 255   // los puntos apagados quedan transparentes
        }

        guard let proveedor = CGDataProvider(data: Data(rgba) as CFData) else { return nil }
        return CGImage(
            width: ancho, height: alto,
            bitsPerComponent: 8, bitsPerPixel: 32, bytesPerRow: ancho * 4,
            space: CGColorSpaceCreateDeviceRGB(),
            bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue),
            provider: proveedor, decode: nil,
            shouldInterpolate: false, intent: .defaultIntent
        )
    }
}
