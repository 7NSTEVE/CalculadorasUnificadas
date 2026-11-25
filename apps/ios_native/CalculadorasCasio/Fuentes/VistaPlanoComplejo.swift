import SwiftUI

/// Plano complejo con los fasores de la pantalla.
///
/// Lo que se dibuja sale de leer el LCD, no de calcular nada aquí. Si el dibujo y la
/// pantalla no coincidieran, manda la pantalla: es la Casio de verdad.
///
/// Para qué sirve: en alterna, `-3+4i` y `5∠126,87°` son el mismo número y la Casio
/// enseña uno de los dos según el modo. El que hace falta para entender el circuito
/// casi siempre es el otro.
struct VistaPlanoComplejo: View {
    /// Fasores a dibujar, del más reciente al más antiguo.
    let fasores: [(etiqueta: String, valor: Complejo)]

    /// Cuál está resaltado. El resto se dibujan apagados, como referencia.
    var destacado: Int = 0

    var body: some View {
        VStack(spacing: 10) {
            GeometryReader { geometria in
                let lado = min(geometria.size.width, geometria.size.height)
                Canvas { contexto, _ in
                    let dibujo = DibujoFasor.para(fasores.map(\.valor), lado: lado)
                    dibujarRejilla(contexto, dibujo: dibujo, lado: lado)
                    dibujarEjes(contexto, dibujo: dibujo, lado: lado)
                    for (indice, fasor) in fasores.enumerated().reversed() {
                        dibujarFasor(contexto, dibujo: dibujo, complejo: fasor.valor,
                                     principal: indice == destacado)
                    }
                }
                .frame(width: lado, height: lado)
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
            .aspectRatio(1, contentMode: .fit)

            if let actual = fasores.indices.contains(destacado) ? fasores[destacado] : nil {
                lectura(actual)
            }
        }
    }

    // MARK: - Lectura numérica

    /// Las dos formas, siempre las dos. Es el punto de la vista.
    private func lectura(_ fasor: (etiqueta: String, valor: Complejo)) -> some View {
        VStack(spacing: 4) {
            if !fasor.etiqueta.isEmpty {
                Text(fasor.etiqueta)
                    .font(.system(size: 10, weight: .bold))
                    .foregroundStyle(.secondary)
            }
            HStack(spacing: 16) {
                VStack(spacing: 2) {
                    Text("rectangular").font(.system(size: 9)).foregroundStyle(.secondary)
                    Text(fasor.valor.rectangular())
                        .font(.system(.callout, design: .monospaced).weight(.semibold))
                }
                VStack(spacing: 2) {
                    Text("polar").font(.system(size: 9)).foregroundStyle(.secondary)
                    Text(fasor.valor.polar())
                        .font(.system(.callout, design: .monospaced).weight(.semibold))
                }
            }
            .textSelection(.enabled)
        }
    }

    // MARK: - Dibujo

    private func dibujarRejilla(_ contexto: GraphicsContext,
                                dibujo: DibujoFasor, lado: CGFloat) {
        let mayor = fasores.map(\.valor.modulo).max() ?? 1
        let paso = DibujoFasor.pasoRejilla(maximo: mayor)
        guard paso > 0, dibujo.escala > 0 else { return }

        let separacion = paso * dibujo.escala
        guard separacion > 4 else { return }   // más junta que esto es una mancha gris

        var trazo = Path()
        var desplazamiento = separacion
        while desplazamiento < Double(lado) / 2 {
            for signo in [-1.0, 1.0] {
                let x = dibujo.centro.x + signo * desplazamiento
                let y = dibujo.centro.y + signo * desplazamiento
                trazo.move(to: CGPoint(x: x, y: 0))
                trazo.addLine(to: CGPoint(x: x, y: Double(lado)))
                trazo.move(to: CGPoint(x: 0, y: y))
                trazo.addLine(to: CGPoint(x: Double(lado), y: y))
            }
            desplazamiento += separacion
        }
        contexto.stroke(trazo, with: .color(.white.opacity(0.08)), lineWidth: 1)
    }

    private func dibujarEjes(_ contexto: GraphicsContext,
                             dibujo: DibujoFasor, lado: CGFloat) {
        var ejes = Path()
        ejes.move(to: CGPoint(x: 0, y: dibujo.centro.y))
        ejes.addLine(to: CGPoint(x: Double(lado), y: dibujo.centro.y))
        ejes.move(to: CGPoint(x: dibujo.centro.x, y: 0))
        ejes.addLine(to: CGPoint(x: dibujo.centro.x, y: Double(lado)))
        contexto.stroke(ejes, with: .color(.white.opacity(0.28)), lineWidth: 1)

        // `foregroundStyle` sobre un Text es iOS 17, y aqui el objetivo es iOS 16.
        // `foregroundColor` hace lo mismo y existe desde mucho antes.
        contexto.draw(Text("Re").font(.system(size: 9)).foregroundColor(.secondary),
                      at: CGPoint(x: Double(lado) - 12, y: dibujo.centro.y - 9))
        contexto.draw(Text("Im").font(.system(size: 9)).foregroundColor(.secondary),
                      at: CGPoint(x: dibujo.centro.x + 12, y: 9))
    }

    private func dibujarFasor(_ contexto: GraphicsContext, dibujo: DibujoFasor,
                              complejo: Complejo, principal: Bool) {
        let punta = dibujo.punto(complejo)
        let origen = CGPoint(x: dibujo.centro.x, y: dibujo.centro.y)
        let destino = CGPoint(x: punta.x, y: punta.y)
        let color: Color = principal ? .orange : .white.opacity(0.35)

        var linea = Path()
        linea.move(to: origen)
        linea.addLine(to: destino)
        contexto.stroke(linea, with: .color(color), lineWidth: principal ? 2.5 : 1.5)

        // Proyecciones a los ejes, punteadas. Es lo que hace ver de un golpe cuánto
        // vale la parte real y cuánto la imaginaria.
        if principal {
            var guias = Path()
            guias.move(to: destino)
            guias.addLine(to: CGPoint(x: destino.x, y: origen.y))
            guias.move(to: destino)
            guias.addLine(to: CGPoint(x: origen.x, y: destino.y))
            contexto.stroke(guias, with: .color(.orange.opacity(0.4)),
                            style: StrokeStyle(lineWidth: 1, dash: [3, 3]))
        }

        // Punta de flecha, orientada según el fasor.
        let angulo = atan2(destino.y - origen.y, destino.x - origen.x)
        let largo: CGFloat = principal ? 10 : 7
        let apertura: CGFloat = 0.4
        var flecha = Path()
        flecha.move(to: destino)
        flecha.addLine(to: CGPoint(x: destino.x - largo * cos(angulo - apertura),
                                   y: destino.y - largo * sin(angulo - apertura)))
        flecha.addLine(to: CGPoint(x: destino.x - largo * cos(angulo + apertura),
                                   y: destino.y - largo * sin(angulo + apertura)))
        flecha.closeSubpath()
        contexto.fill(flecha, with: .color(color))
    }
}

/// Hoja que enseña el plano complejo de lo que hay ahora en la calculadora.
struct VistaVisualizador: View {
    @ObservedObject var emulador: EmuladorCalculadora
    @ObservedObject var sesion: SesionEstudio
    @Environment(\.dismiss) private var cerrar

    /// Lo que se puede dibujar de la pantalla actual y de la cinta reciente.
    ///
    /// Se mira primero la pantalla, porque es lo que el usuario está viendo; después
    /// la cinta, para poder comparar el fasor de ahora con los anteriores.
    private var fasores: [(etiqueta: String, valor: Complejo)] {
        var encontrados: [(etiqueta: String, valor: Complejo)] = []

        for linea in emulador.lineasPantalla.reversed() {
            if case .complejo(let z) = LectorContenido.interpretar(linea) {
                encontrados.append((etiqueta: "en pantalla", valor: z))
                break
            }
        }
        for entrada in sesion.cinta.prefix(4) {
            if case .complejo(let z) = LectorContenido.interpretar(entrada.resultado) {
                encontrados.append((etiqueta: entrada.teclas, valor: z))
            }
        }
        return encontrados
    }

    var body: some View {
        NavigationView {
            Group {
                if fasores.isEmpty {
                    VStack(spacing: 12) {
                        Image(systemName: "point.topleft.down.curvedto.point.bottomright.up")
                            .font(.largeTitle)
                            .foregroundStyle(.secondary)
                        Text("No hay ningún complejo en pantalla.")
                            .font(.callout)
                        Text("Entra en modo CMPLX y calcula algo como 3+4i. "
                             + "Lo que se dibuje sale de leer el LCD, no de recalcularlo.")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .multilineTextAlignment(.center)
                            .padding(.horizontal, 30)
                    }
                } else {
                    VistaPlanoComplejo(fasores: fasores).padding()
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(Color(white: 0.13))
            .foregroundStyle(.white)
            .navigationTitle("Plano complejo")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Listo") { cerrar() }
                }
            }
        }
    }
}
