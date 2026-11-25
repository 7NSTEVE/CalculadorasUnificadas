import SwiftUI
import UIKit

/// Tarjeta de un cálculo, para pegar en OneNote.
///
/// Lleva la pantalla auténtica de la Casio, no una recreación con la tipografía del
/// sistema: los píxeles son los que el firmware escribió en la RAM de vídeo.
///
/// Y lleva el modo con el que se calculó. Eso es lo que, meses después, evita la duda
/// de "¿esto lo saqué en grados o en radianes?".
struct TarjetaCalculo: View {
    let entrada: EntradaCinta
    let objetivo: String
    let calculadora: String

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            if !objetivo.isEmpty {
                Text(objetivo)
                    .font(.footnote.weight(.semibold))
                    .foregroundStyle(.secondary)
            }

            PantallaLCD(pixeles: entrada.pixeles,
                        ancho: entrada.ancho, alto: entrada.alto)
                .frame(width: CGFloat(entrada.ancho) * 2,
                       height: CGFloat(entrada.alto) * 2)
                .overlay(RoundedRectangle(cornerRadius: 3)
                    .strokeBorder(Color.black.opacity(0.25), lineWidth: 1))

            Text("\(entrada.teclas)  =  \(entrada.resultado)")
                .font(.system(.body, design: .monospaced).weight(.medium))
                .foregroundStyle(.black)

            Text(pieDePagina)
                .font(.system(size: 10))
                .foregroundStyle(.secondary)
        }
        .padding(14)
        .background(Color.white)
    }

    var pieDePagina: String {
        entrada.modo.isEmpty ? calculadora : "\(calculadora) · \(entrada.modo)"
    }

    /// Texto ya formateado, con los símbolos matemáticos de verdad.
    static func texto(entrada: EntradaCinta, objetivo: String, calculadora: String) -> String {
        var lineas: [String] = []
        if !objetivo.isEmpty { lineas.append(objetivo) }
        lineas.append("\(entrada.teclas) = \(entrada.resultado)")
        let contexto = entrada.modo.isEmpty ? "" : " · \(entrada.modo)"
        lineas.append("(\(calculadora)\(contexto))")
        return lineas.joined(separator: "\n")
    }

    /// Rasteriza la tarjeta para poder pegarla como imagen.
    @MainActor
    static func imagen(entrada: EntradaCinta, objetivo: String, calculadora: String) -> UIImage? {
        let renderizador = ImageRenderer(content:
            TarjetaCalculo(entrada: entrada, objetivo: objetivo, calculadora: calculadora))
        // x3 para que se vea nítida en pantallas Retina y al ampliarla en OneNote.
        renderizador.scale = 3
        return renderizador.uiImage
    }

    /// Lo mismo, pero para arrastrarlo a OneNote en vez de copiarlo.
    ///
    /// Se registran las dos formas y elige la app de destino: OneNote se queda con
    /// la imagen, un campo de texto con el texto. Asi el mismo gesto vale para todo.
    @MainActor
    static func proveedor(entrada: EntradaCinta, objetivo: String,
                          calculadora: String) -> NSItemProvider {
        let proveedor = NSItemProvider()
        if let imagen = imagen(entrada: entrada, objetivo: objetivo, calculadora: calculadora) {
            proveedor.registerObject(imagen, visibility: .all)
        }
        let texto = texto(entrada: entrada, objetivo: objetivo, calculadora: calculadora)
        proveedor.registerObject(texto as NSString, visibility: .all)
        return proveedor
    }

    /// Solo la pantalla, sin la tarjeta alrededor.
    @MainActor
    static func imagenDePantalla(entrada: EntradaCinta) -> UIImage? {
        let renderizador = ImageRenderer(content:
            PantallaLCD(pixeles: entrada.pixeles, ancho: entrada.ancho, alto: entrada.alto)
                .frame(width: CGFloat(entrada.ancho) * 4,
                       height: CGFloat(entrada.alto) * 4))
        renderizador.scale = 2
        return renderizador.uiImage
    }
}
