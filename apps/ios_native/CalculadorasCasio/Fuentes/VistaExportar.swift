import SwiftUI
import UIKit

/// Exportar la sesión a lo que espera OneNote.
///
/// La vista previa no es decorativa: pegar en el cuaderno algo que no era lo que se
/// creía y descubrirlo una semana después es exactamente el problema que esta app
/// intenta quitar de en medio.
struct VistaExportar: View {
    @ObservedObject var sesion: SesionEstudio
    let nombreModelo: String

    @Environment(\.dismiss) private var cerrar

    @State private var formato: FormatoExportacion = .markdown
    @State private var alcance: AlcanceExportacion = .todo
    @State private var avisoCopiado = false

    private var lineas: [LineaExportable] {
        sesion.lineasExportables(alcance: alcance)
    }

    private var documento: String {
        Exportacion.documento(lineas, formato: formato,
                              objetivo: sesion.objetivo, modelo: nombreModelo)
    }

    var body: some View {
        NavigationView {
            Form {
                Section("Qué se lleva") {
                    Picker("Alcance", selection: $alcance) {
                        ForEach(AlcanceExportacion.allCases) { opcion in
                            Text(opcion.nombre).tag(opcion)
                        }
                    }
                    .pickerStyle(.segmented)

                    if alcance == .soloDestacado && lineas.isEmpty {
                        Text("No has destacado ningún resultado todavía. "
                             + "Mantén pulsada una línea de la cinta y marca la estrella.")
                            .font(.caption)
                            .foregroundStyle(.orange)
                    }
                }

                Section("Materia y ejercicio") {
                    HStack {
                        Text("Materia")
                        Spacer()
                        TextField("Circuitos", text: $sesion.materia)
                            .multilineTextAlignment(.trailing)
                            .autocorrectionDisabled()
                    }
                    Stepper("Ejercicio \(sesion.ejercicio)",
                            value: $sesion.ejercicio, in: 1...99)
                    if let primera = lineas.first {
                        Text("Las referencias saldrán como \(primera.referencia)")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }

                Section("Formato") {
                    Picker("Formato", selection: $formato) {
                        ForEach(FormatoExportacion.allCases) { opcion in
                            Text(opcion.nombre).tag(opcion)
                        }
                    }
                    .pickerStyle(.inline)
                    .labelsHidden()
                    Text(formato.pista)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                Section("Vista previa") {
                    ScrollView(.horizontal, showsIndicators: true) {
                        Text(documento.isEmpty ? "(nada que exportar)" : documento)
                            .font(.system(size: 11, design: .monospaced))
                            .textSelection(.enabled)
                    }
                    .frame(maxHeight: 220)
                }

                Section {
                    Button {
                        UIPasteboard.general.string = documento
                        avisoCopiado = true
                    } label: {
                        Label("Copiar", systemImage: "doc.on.doc")
                    }
                    .disabled(lineas.isEmpty)

                    ShareLink(item: documento) {
                        Label("Compartir", systemImage: "square.and.arrow.up")
                    }
                    .disabled(lineas.isEmpty)
                }
            }
            .navigationTitle("Exportar")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Cerrar") { cerrar() }
                }
            }
            .overlay(alignment: .bottom) {
                if avisoCopiado {
                    Text("Copiado")
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

/// El botón «me perdí»: objetivo, paso, último resultado y datos sin usar.
///
/// Es una sola pantalla y se cierra con un toque. La idea es no tener que reconstruir
/// nada en la cabeza al volver de otra app.
struct VistaMePerdi: View {
    @ObservedObject var sesion: SesionEstudio
    @Environment(\.dismiss) private var cerrar

    private var lineas: [String] {
        Cadenas.resumenDeSituacion(
            objetivo: sesion.objetivo,
            siguientePaso: sesion.siguientePaso,
            ultimoResultado: sesion.cinta.first?.resultado ?? "",
            sinUsar: Cadenas.variablesSinUsar(
                variables: sesion.variables.map(\.nombre),
                expresiones: sesion.cinta.map(\.teclas)))
    }

    var body: some View {
        NavigationView {
            List {
                ForEach(Array(lineas.enumerated()), id: \.offset) { _, linea in
                    Text(linea).font(.body)
                }
                if !sesion.variables.isEmpty {
                    Section("Datos del enunciado") {
                        ForEach(sesion.variables) { variable in
                            Text(variable.etiqueta)
                                .font(.system(.callout, design: .monospaced))
                        }
                    }
                }
            }
            .navigationTitle("Dónde estabas")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Vale") { cerrar() }
                }
            }
        }
    }
}
