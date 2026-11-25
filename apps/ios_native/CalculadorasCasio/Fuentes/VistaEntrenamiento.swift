import SwiftUI

/// Manual, modo entrenamiento y modo examen (puntos 66, 67, 69, 70).
struct VistaEntrenamiento: View {
    let modelo: ModeloCalculadora
    @Binding var modo: ModoAprendizaje
    @ObservedObject var examen: SesionExamen
    /// Señala las teclas de una ruta sobre la carátula, sin pulsarlas.
    let alSenalar: ([String]) -> Void

    @Environment(\.dismiss) private var cerrar
    @State private var busqueda = ""

    private var clave: String { modelo.hardwareId == 4 ? "classwiz" : "esplus" }

    private var entradas: [EntradaManual] {
        busqueda.isEmpty ? Manual.para(modelo: clave)
                         : Manual.buscar(busqueda, modelo: clave)
    }

    var body: some View {
        NavigationView {
            Form {
                Section {
                    Picker("Modo", selection: $modo) {
                        ForEach(ModoAprendizaje.allCases) { opcion in
                            Text(opcion.nombre).tag(opcion)
                        }
                    }
                    .pickerStyle(.segmented)
                    Text(modo.explicacion)
                        .font(.caption).foregroundStyle(.secondary)
                } header: {
                    Text("Cómo te ayuda la app")
                } footer: {
                    Text("Entrenamiento y examen van a propósito en contra de la "
                         + "comodidad. Si la app teclea siempre por ti, el día del "
                         + "examen no sabrás dónde está ∫.")
                }

                if modo == .examen {
                    seccionExamen
                }

                Section {
                    TextField("buscar: matriz inversa, desviación, integral…",
                              text: $busqueda)
                        .autocorrectionDisabled()
                    ForEach(entradas) { entrada in
                        fila(entrada)
                    }
                    if entradas.isEmpty {
                        Text("Nada con ese nombre.").foregroundStyle(.secondary)
                    }
                } header: {
                    Text("Manual de \(modelo.nombre)")
                } footer: {
                    Text("Las rutas están comprobadas contra la ROM, no copiadas de "
                         + "un manual en PDF.")
                }
            }
            .navigationTitle("Aprender")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Listo") { cerrar() }
                }
            }
        }
    }

    private var seccionExamen: some View {
        Section("Cronómetro") {
            HStack {
                Text(SesionExamen.reloj(examen.segundos))
                    .font(.system(.largeTitle, design: .monospaced).weight(.semibold))
                Spacer()
                if examen.corriendo {
                    Button("Parar") { examen.parar() }.buttonStyle(.bordered)
                } else {
                    Button("Empezar") { examen.empezar() }
                        .buttonStyle(.borderedProminent).tint(.orange)
                }
            }
            if examen.corriendo {
                Button("Ejercicio hecho") { examen.siguienteEjercicio() }
            }
            if !examen.tiempos.isEmpty {
                Text(examen.resumen)
                    .font(.caption).foregroundStyle(.secondary)
            }
        }
    }

    private func fila(_ entrada: EntradaManual) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                Text(entrada.procedimiento.nombre)
                Spacer()
                if !entrada.disponible {
                    Text("no lo trae")
                        .font(.caption2)
                        .padding(.horizontal, 6).padding(.vertical, 2)
                        .background(Color.secondary.opacity(0.2), in: Capsule())
                }
            }
            if let ruta = entrada.ruta {
                Text(ruta.comoSeLee)
                    .font(.system(.caption, design: .monospaced))
                    .foregroundStyle(.secondary)
                if let despues = ruta.despues {
                    Text(despues).font(.caption2).foregroundStyle(.orange)
                }
                // En examen no se señala nada: es justo lo que se está practicando.
                if modo.permiteSenalar {
                    Button {
                        alSenalar(ruta.teclas)
                        cerrar()
                    } label: {
                        Label(modo == .entrenamiento ? "Señalar (la pulsas tú)"
                                                     : "Señalar en el teclado",
                              systemImage: "hand.point.up.left")
                            .font(.caption)
                    }
                    .buttonStyle(.bordered)
                }
            }
        }
    }
}

/// Barra con las teclas que más se usan (punto 110).
///
/// No sustituye a la carátula ni la modifica: manda exactamente los mismos códigos.
/// Es un atajo para no cruzar la pantalla entera cada vez que hace falta un `AC`.
struct BarraEsenciales: View {
    let modelo: ModeloCalculadora
    @ObservedObject var preferencias: Preferencias
    let alPulsar: (TeclaFisica) -> Void

    private var teclas: [TeclaFisica] {
        Preferencias.esencialesPorDefecto.compactMap { nombre in
            modelo.disposicion.teclas.first { $0.nombre == nombre }
        }
    }

    var body: some View {
        if preferencias.barraEsenciales {
            HStack(spacing: 6) {
                // En zurdo la barra se invierte, para que las teclas más usadas
                // queden bajo el pulgar y no al otro lado de la pantalla.
                ForEach(preferencias.lado == .izquierda ? teclas.reversed() : teclas,
                        id: \.codigo) { tecla in
                    Button {
                        alPulsar(tecla)
                    } label: {
                        Text(etiqueta(tecla.nombre))
                            .font(.system(size: 15, weight: .medium, design: .monospaced))
                            .frame(minWidth: 40, minHeight: 40)
                    }
                    .buttonStyle(.bordered)
                }
            }
            .padding(.horizontal, 8).padding(.vertical, 6)
            .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))
        }
    }

    private func etiqueta(_ nombre: String) -> String {
        switch nombre {
        case "MAS": return "+"
        case "MENOS": return "−"
        case "POR": return "×"
        case "ENTRE": return "÷"
        case "IGUAL": return "="
        case "PAREN_ABRE": return "("
        case "PAREN_CIERRA": return ")"
        default: return nombre
        }
    }
}
