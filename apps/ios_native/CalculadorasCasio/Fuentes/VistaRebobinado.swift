import SwiftUI

/// Barra temporal para moverse por el ejercicio tecla a tecla.
///
/// Antes esto no se podía ofrecer: cada movimiento del dedo habría costado una
/// reproducción entera desde el encendido, a ~20 ms por tecla. Con el volcado del
/// chipset cuesta décimas de milisegundo, así que se puede arrastrar y ver la
/// pantalla de cada momento al vuelo.
///
/// La barra solo cubre el tramo que se puede restaurar al instante. Más atrás sigue
/// existiendo, pero cuesta reproducir, y una barra que promete lo que no puede
/// cumplir es peor que no tenerla.
struct VistaRebobinado: View {
    @ObservedObject var emulador: EmuladorCalculadora

    /// Posición mientras se arrastra. Se separa del historial porque el arrastre va
    /// más rápido que la publicación del estado.
    @State private var posicion: Double = 0
    @State private var arrastrando = false

    private var maximo: Double { Double(emulador.historial.count) }
    private var minimo: Double { Double(emulador.primeraPosicionInmediata) }

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text("REBOBINAR")
                    .font(.system(size: 10, weight: .bold))
                    .foregroundStyle(.secondary)
                Spacer()
                Text(rotulo)
                    .font(.system(size: 11, design: .monospaced))
                    .foregroundStyle(arrastrando ? Color.orange : Color.secondary)
            }

            if maximo > minimo {
                Slider(value: $posicion, in: minimo...maximo, step: 1) { editando in
                    arrastrando = editando
                    // Al soltar se fija de verdad. Mientras se arrastra también se
                    // aplica, porque el sentido de la barra es VER la pantalla de
                    // aquel momento, no elegir a ciegas.
                    if !editando { emulador.rebobinarA(posicion: Int(posicion)) }
                }
                .tint(.orange)
                .onChange(of: posicion) { nueva in
                    guard arrastrando else { return }
                    emulador.rebobinarA(posicion: Int(nueva))
                }
                .disabled(emulador.rebobinando)
            } else {
                Text("Todavía no hay teclas que deshacer.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            botonesRapidos
            if !emulador.checkpoints.isEmpty { listaCheckpoints }
        }
        .onAppear { posicion = maximo }
        .onChange(of: emulador.historial.count) { nuevo in
            // Si no se está arrastrando, la barra sigue al final: es donde está la
            // calculadora de verdad.
            if !arrastrando { posicion = Double(nuevo) }
        }
    }

    private var rotulo: String {
        let actual = Int(posicion)
        let total = emulador.historial.count
        if actual == total { return "al día · \(total) teclas" }
        return "\(total - actual) atrás · tecla \(actual) de \(total)"
    }

    private var botonesRapidos: some View {
        HStack(spacing: 6) {
            boton("1", pasos: 1)
            boton("5", pasos: 5)
            boton("20", pasos: 20)
            Spacer(minLength: 0)
            Button {
                emulador.volverAntesDelUltimoBorrado()
            } label: {
                Text("antes del AC").font(.system(size: 11, weight: .medium))
            }
            .buttonStyle(.bordered)
            .tint(.orange)
            .disabled(!emulador.checkpoints.contains { $0.motivo == .antesDeBorrar })
        }
    }

    private func boton(_ texto: String, pasos: Int) -> some View {
        Button {
            emulador.rebobinar(pasos: pasos)
        } label: {
            Text("−\(texto)").font(.system(size: 11, design: .monospaced))
        }
        .buttonStyle(.bordered)
        .disabled(emulador.pasosDeshacer < pasos || emulador.rebobinando)
    }

    private var listaCheckpoints: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text("PUNTOS AUTOMÁTICOS")
                .font(.system(size: 10, weight: .bold))
                .foregroundStyle(.secondary)
                .padding(.top, 4)
            // Del más reciente al más antiguo: al que se quiere volver casi siempre
            // es al último.
            ForEach(emulador.checkpoints.reversed()) { punto in
                Button {
                    emulador.volverA(punto)
                } label: {
                    HStack(spacing: 8) {
                        Image(systemName: punto.motivo == .antesDeBorrar
                              ? "exclamationmark.triangle.fill" : "clock.arrow.circlepath")
                            .font(.caption2)
                            .foregroundStyle(punto.motivo == .antesDeBorrar
                                             ? Color.orange : Color.secondary)
                        Text(punto.motivo.rawValue)
                            .font(.system(size: 11))
                        Spacer(minLength: 4)
                        Text("\(punto.teclas) teclas")
                            .font(.system(size: 10, design: .monospaced))
                            .foregroundStyle(.secondary)
                    }
                    .padding(.horizontal, 8)
                    .padding(.vertical, 5)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(Color(white: 0.20), in: RoundedRectangle(cornerRadius: 7))
                }
                .buttonStyle(.plain)
            }
        }
    }
}

/// Aviso de que hay un `AC` retenido, con la salida a mano.
///
/// Aparece encima de todo pero sin robar el foco: si no se toca, el `AC` se aplica
/// solo al cumplirse el retardo, que es lo que espera quien lo pulsó a propósito.
struct AvisoBorradoPendiente: View {
    @ObservedObject var emulador: EmuladorCalculadora

    var body: some View {
        if emulador.borradoPendiente != nil {
            HStack(spacing: 10) {
                Image(systemName: "clock.badge.exclamationmark")
                Text("Borrando…")
                    .font(.footnote.weight(.medium))
                Button("Cancelar") { emulador.cancelarBorrado() }
                    .font(.footnote.weight(.semibold))
                Button("Ya") { emulador.confirmarBorrado() }
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
            .padding(.horizontal, 14).padding(.vertical, 8)
            .background(.ultraThinMaterial, in: Capsule())
            .transition(.move(edge: .top).combined(with: .opacity))
        }
    }
}
