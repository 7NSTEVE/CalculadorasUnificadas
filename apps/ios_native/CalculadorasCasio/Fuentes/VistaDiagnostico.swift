import SwiftUI
import UIKit

/// Qué máquina es esta exactamente, y cómo mandar un fallo para que se reproduzca.
///
/// La etiqueta de fidelidad está redactada con cuidado a propósito: dice lo que se ha
/// comprobado y admite lo que no. Es la diferencia entre decir la verdad y vender
/// humo, y esta app se apoya entera en que el número sea de fiar.
struct VistaDiagnostico: View {
    let modelo: ModeloCalculadora
    @ObservedObject var emulador: EmuladorCalculadora
    @Environment(\.dismiss) private var cerrar

    @State private var nota = ""
    @State private var avisoCopiado = false

    private var ficha: FichaFidelidad {
        FichaFidelidad(
            modelo: modelo.nombre,
            romBytes: Paquete.datos(modelo.recursoRom, extension: "bin")?.count ?? 0,
            romHuella: Diagnostico.huellaDeModelo(modelo.recursoRom),
            // `hardwareId` y `csrMask` vienen del interop en C, donde son Int32.
            hardwareId: Int(modelo.hardwareId),
            csrMask: Int(modelo.csrMask),
            hardwareReal: modelo.realHardware != 0,
            pantalla: "\(emulador.ancho)×\(emulador.alto)")
    }

    private var informe: InformeFallo {
        InformeFallo(modelo: modelo.nombre,
                     romHuella: ficha.romHuella,
                     teclas: emulador.historial,
                     pantallaFinal: emulador.lineasPantalla,
                     momento: Date(),
                     nota: nota)
    }

    var body: some View {
        NavigationView {
            Form {
                Section {
                    ForEach(ficha.lineas, id: \.0) { fila in
                        HStack {
                            Text(fila.0)
                            Spacer()
                            Text(fila.1)
                                .font(.system(.footnote, design: .monospaced))
                                .foregroundStyle(.secondary)
                                .textSelection(.enabled)
                        }
                    }
                } header: {
                    Text("Identidad de la máquina")
                } footer: {
                    Text(ficha.afirmacion)
                }

                Section {
                    TextField("qué esperabas y qué salió", text: $nota, axis: .vertical)
                        .lineLimit(2...4)
                    Button {
                        UIPasteboard.general.string = informe.texto
                        avisoCopiado = true
                    } label: {
                        Label("Copiar informe reproducible", systemImage: "ant")
                    }
                    ShareLink(item: informe.texto) {
                        Label("Compartir", systemImage: "square.and.arrow.up")
                    }
                } header: {
                    Text("Reportar un cálculo raro")
                } footer: {
                    Text("Se copia la huella de la ROM, las \(emulador.historial.count) "
                         + "teclas desde el encendido y la pantalla final. Con eso el "
                         + "caso se reproduce exactamente en otra máquina. No se "
                         + "incluye nada más.")
                }

                Section("Estado ahora") {
                    HStack {
                        Text("Teclas pulsadas")
                        Spacer()
                        Text("\(emulador.historial.count)")
                            .font(.system(.footnote, design: .monospaced))
                            .foregroundStyle(.secondary)
                    }
                    HStack {
                        Text("Puntos de retorno")
                        Spacer()
                        Text("\(emulador.checkpoints.count)")
                            .font(.system(.footnote, design: .monospaced))
                            .foregroundStyle(.secondary)
                    }
                    HStack {
                        Text("Rebobinado inmediato desde")
                        Spacer()
                        Text("tecla \(emulador.primeraPosicionInmediata)")
                            .font(.system(.footnote, design: .monospaced))
                            .foregroundStyle(.secondary)
                    }
                }
            }
            .navigationTitle("Diagnóstico")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Listo") { cerrar() }
                }
            }
            .overlay(alignment: .bottom) {
                if avisoCopiado {
                    Text("Informe copiado")
                        .font(.footnote.weight(.medium))
                        .padding(.horizontal, 14).padding(.vertical, 8)
                        .background(.ultraThinMaterial, in: Capsule())
                        .padding(.bottom, 20)
                        .task {
                            try? await Task.sleep(nanoseconds: 1_400_000_000)
                            avisoCopiado = false
                        }
                }
            }
            .animation(.easeInOut(duration: 0.2), value: avisoCopiado)
        }
    }
}
