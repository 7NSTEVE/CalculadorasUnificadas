import SwiftUI

/// Paleta de comandos (`⌘K`) y tira de teclas pulsadas.
///
/// La paleta existe porque el menú `…` ya no cabe: hay una docena de acciones y
/// buscarlas en una lista desplegable es más lento que escribir tres letras. Y en el
/// iPad con funda-teclado, escribir es lo natural.

struct PaletaComandos: View {
    /// Se llama con la clave del comando elegido.
    let alElegir: (String) -> Void

    @Environment(\.dismiss) private var cerrar
    @State private var busqueda = ""

    private var encontrados: [Comando] { Comandos.buscar(busqueda) }

    var body: some View {
        NavigationView {
            List {
                Section {
                    TextField("qué quieres hacer", text: $busqueda)
                        .autocorrectionDisabled()
                        .textInputAutocapitalization(.never)
                }
                ForEach(encontrados) { comando in
                    Button {
                        alElegir(comando.clave)
                        cerrar()
                    } label: {
                        HStack {
                            Label(comando.titulo, systemImage: comando.simbolo)
                            Spacer()
                            if !comando.atajoLegible.isEmpty {
                                Text(comando.atajoLegible)
                                    .font(.system(.footnote, design: .monospaced))
                                    .foregroundStyle(.secondary)
                            }
                        }
                    }
                }
                if encontrados.isEmpty {
                    Text("Nada con ese nombre.").foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Comandos")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cerrar") { cerrar() }
                }
            }
        }
    }
}

/// Tira grande con las últimas teclas, para explicar en clase (puntos 133-135).
///
/// Se puede apagar entera: en un examen, tener las teclas escritas en pantalla llama
/// la atención de quien pasa por al lado, y eso no le conviene a nadie.
struct TiraTeclas: View {
    @ObservedObject var registro: RegistroTeclas

    var body: some View {
        if registro.visible && !registro.tira.isEmpty {
            HStack(spacing: 6) {
                ForEach(registro.tira) { pulsacion in
                    Text(etiqueta(pulsacion.nombre))
                        .font(.system(size: 20, weight: .semibold, design: .monospaced))
                        .padding(.horizontal, 10).padding(.vertical, 6)
                        .background(Color(white: 0.22), in: RoundedRectangle(cornerRadius: 8))
                }
            }
            .padding(.horizontal, 12).padding(.vertical, 8)
            .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))
            .transition(.opacity)
        }
    }

    /// Los nombres internos son legibles para el código, no para quien mira.
    /// `PAREN_ABRE` no se entiende de un vistazo; `(` sí.
    private func etiqueta(_ nombre: String) -> String {
        switch nombre {
        case "MAS": return "+"
        case "MENOS": return "−"
        case "POR": return "×"
        case "ENTRE": return "÷"
        case "IGUAL": return "="
        case "PAREN_ABRE": return "("
        case "PAREN_CIERRA": return ")"
        case "COMA": return "."
        case "CUADRADO": return "x²"
        case "POTENCIA": return "^"
        case "RAIZ": return "√"
        case "NEGATIVO": return "(−)"
        case "IZQUIERDA": return "◀"
        case "DERECHA": return "▶"
        case "ARRIBA": return "▲"
        case "ABAJO": return "▼"
        default: return nombre
        }
    }
}

/// Guion del ejercicio: como mucho tres pasos a la vista (puntos 19 y 20).
struct VistaGuion: View {
    @Binding var guion: GuionEjercicio
    /// Si solo se enseña el paso actual.
    @Binding var soloUno: Bool

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text("PASOS")
                    .font(.system(size: 10, weight: .bold))
                    .foregroundStyle(.secondary)
                Spacer()
                Toggle("solo uno", isOn: $soloUno)
                    .labelsHidden()
                    .scaleEffect(0.7)
            }

            if guion.pasos.isEmpty {
                Text("Escribe los pasos del ejercicio y ve tachándolos.")
                    .font(.caption).foregroundStyle(.secondary)
            } else if guion.terminado {
                Text("Ejercicio terminado.")
                    .font(.callout.weight(.medium))
                    .foregroundStyle(.green)
            } else if soloUno, let paso = guion.soloSiguiente {
                fila(texto: paso, actual: true)
            } else {
                ForEach(guion.visibles(), id: \.indice) { entrada in
                    fila(texto: entrada.texto, actual: entrada.indice == guion.actual)
                }
            }

            if !guion.pasos.isEmpty && !guion.terminado {
                HStack(spacing: 8) {
                    Button("Hecho") { guion.avanzar() }
                        .buttonStyle(.borderedProminent).tint(.green)
                        .font(.caption)
                    Button("Atrás") { guion.retroceder() }
                        .buttonStyle(.bordered)
                        .font(.caption)
                        .disabled(guion.actual == 0)
                }
            }
        }
    }

    private func fila(texto: String, actual: Bool) -> some View {
        HStack(alignment: .top, spacing: 8) {
            Image(systemName: actual ? "arrow.right.circle.fill" : "circle")
                .font(.caption)
                .foregroundStyle(actual ? Color.orange : Color.secondary)
            Text(texto)
                .font(actual ? .callout.weight(.semibold) : .caption)
                .foregroundStyle(actual ? Color.white : Color.secondary)
        }
    }
}

/// Selector de materia, con lo que cada una recuerda (puntos 35, 36, 120).
struct VistaMaterias: View {
    @ObservedObject var cuaderno: Cuaderno
    @Environment(\.dismiss) private var cerrar

    @State private var nombreNuevo = ""
    @State private var busqueda = ""

    var body: some View {
        NavigationView {
            List {
                Section("Materia activa") {
                    ForEach(cuaderno.materias) { materia in
                        Button {
                            cuaderno.materiaActiva = materia.nombre
                        } label: {
                            HStack {
                                VStack(alignment: .leading, spacing: 2) {
                                    Text(materia.nombre)
                                    Text("ref \(materia.prefijo) · ejercicio \(materia.ultimoEjercicio)"
                                         + (materia.modeloPreferido.map { " · \($0)" } ?? ""))
                                        .font(.caption).foregroundStyle(.secondary)
                                }
                                Spacer()
                                if materia.nombre == cuaderno.materiaActiva {
                                    Image(systemName: "checkmark").foregroundStyle(.orange)
                                }
                            }
                        }
                    }
                    HStack {
                        TextField("materia nueva", text: $nombreNuevo)
                            .autocorrectionDisabled()
                        Button("Crear") {
                            cuaderno.crearMateria(nombreNuevo)
                            nombreNuevo = ""
                        }
                        .disabled(nombreNuevo.trimmingCharacters(in: .whitespaces).isEmpty)
                    }
                }

                Section {
                    TextField("buscar por resultado, objetivo o título", text: $busqueda)
                        .autocorrectionDisabled()
                    ForEach(cuaderno.buscar(busqueda)) { sesion in
                        HStack(alignment: .top) {
                            VStack(alignment: .leading, spacing: 2) {
                                Text(sesion.titulo)
                                Text("\(sesion.materia) · \(sesion.resultados.joined(separator: ", "))")
                                    .font(.caption).foregroundStyle(.secondary)
                                    .lineLimit(1)
                            }
                            Spacer()
                            Button {
                                cuaderno.alternarFavorita(sesion.id)
                            } label: {
                                Image(systemName: sesion.favorita ? "star.fill" : "star")
                                    .foregroundStyle(sesion.favorita ? Color.yellow : Color.secondary)
                            }
                            .buttonStyle(.plain)
                        }
                    }
                } header: {
                    Text("Cuaderno")
                } footer: {
                    Text("Se busca también por el resultado. Es lo que de verdad se "
                         + "recuerda: «aquello que daba 4,7 kΩ», no cómo se llamaba.")
                }
            }
            .navigationTitle("Materias")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Listo") { cerrar() }
                }
            }
        }
    }
}
