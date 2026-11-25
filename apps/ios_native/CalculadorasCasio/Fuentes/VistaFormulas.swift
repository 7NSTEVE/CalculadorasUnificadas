import SwiftUI

/// Biblioteca de fórmulas con sustitución.
///
/// Lo que sale de aquí no es un resultado: es una EXPRESIÓN que se teclea en la
/// Casio. Quien calcula sigue siendo la calculadora emulada, que es justo lo que
/// hace que el número sea de fiar.
struct VistaFormulas: View {
    let modelo: TraductorExpresion.Modelo
    /// Se llama con la expresión ya sustituida, lista para enviar al teclado.
    let alEnviar: (String) -> Void

    @Environment(\.dismiss) private var cerrar

    @State private var busqueda = ""
    @State private var seleccionada: Formula?
    @State private var incognita = ""
    @State private var valores: [String: String] = [:]
    @State private var aviso: String?

    var body: some View {
        NavigationView {
            Group {
                if let formula = seleccionada {
                    detalle(formula)
                } else {
                    listado
                }
            }
            .navigationTitle(seleccionada?.nombre ?? "Fórmulas")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    if seleccionada != nil {
                        Button("Fórmulas") { seleccionada = nil }
                    } else {
                        Button("Cerrar") { cerrar() }
                    }
                }
            }
        }
    }

    // MARK: - Listado

    private var listado: some View {
        List {
            Section {
                TextField("buscar por nombre, materia o símbolo (Xc, τ…)", text: $busqueda)
                    .autocorrectionDisabled()
                    .textInputAutocapitalization(.never)
            } footer: {
                Text("Buscar por símbolo es lo que sirve en un examen: el enunciado "
                     + "te da «Xc» y no el nombre de la fórmula.")
            }

            ForEach(agrupadas, id: \.materia) { grupo in
                Section(grupo.materia) {
                    ForEach(grupo.formulas) { formula in
                        Button {
                            abrir(formula)
                        } label: {
                            VStack(alignment: .leading, spacing: 3) {
                                Text(formula.nombre).font(.body)
                                Text(formula.expresion)
                                    .font(.system(.caption, design: .monospaced))
                                    .foregroundStyle(.secondary)
                            }
                        }
                    }
                }
            }
        }
    }

    private var agrupadas: [(materia: String, formulas: [Formula])] {
        let encontradas = Formulas.buscar(busqueda)
        // Se conserva el orden del catálogo dentro de cada materia: está puesto de
        // más usada a menos, y ordenar alfabéticamente lo estropearía.
        var materias: [String] = []
        for formula in encontradas where !materias.contains(formula.materia) {
            materias.append(formula.materia)
        }
        return materias.map { materia in
            (materia, encontradas.filter { $0.materia == materia })
        }
    }

    private func abrir(_ formula: Formula) {
        seleccionada = formula
        incognita = formula.despejables.first?.simbolo ?? ""
        valores = [:]
        aviso = nil
    }

    // MARK: - Detalle

    private func detalle(_ formula: Formula) -> some View {
        Form {
            Section {
                Text(formula.expresion)
                    .font(.system(.title3, design: .monospaced))
                    .frame(maxWidth: .infinity, alignment: .center)
                    .padding(.vertical, 6)
            }

            Section("Qué buscas") {
                Picker("Incógnita", selection: $incognita) {
                    ForEach(formula.despejables) { variable in
                        Text("\(variable.simbolo) — \(variable.descripcion)")
                            .tag(variable.simbolo)
                    }
                }
                .pickerStyle(.inline)
                .labelsHidden()
            }

            Section {
                ForEach(formula.variables.filter { $0.simbolo != incognita }) { variable in
                    HStack {
                        VStack(alignment: .leading, spacing: 2) {
                            Text(variable.simbolo)
                                .font(.system(.body, design: .monospaced).weight(.semibold))
                            Text(variable.descripcion)
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                        Spacer(minLength: 8)
                        TextField(variable.unidad.isEmpty ? "valor" : variable.unidad,
                                  text: enlace(variable.simbolo))
                            .multilineTextAlignment(.trailing)
                            .keyboardType(.numbersAndPunctuation)
                            .autocorrectionDisabled()
                            .frame(maxWidth: 130)
                        if !variable.unidad.isEmpty {
                            Text(variable.unidad)
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                    }
                }
            } header: {
                Text("Datos")
            } footer: {
                Text("Se admiten sufijos: 4,7k son 4700 y 100n son 1e-7. Es donde más "
                     + "se falla, y no da error, solo da mal el resultado.")
            }

            if let aviso {
                Section { Text(aviso).font(.callout).foregroundStyle(.orange) }
            }

            Section {
                Button {
                    enviar(formula)
                } label: {
                    Label("Teclear en la calculadora", systemImage: "arrow.right.circle.fill")
                }
            }
        }
    }

    /// Enlace de texto a `valores`, que es un diccionario y no tiene binding propio.
    private func enlace(_ simbolo: String) -> Binding<String> {
        Binding(get: { valores[simbolo] ?? "" },
                set: { valores[simbolo] = $0 })
    }

    private func enviar(_ formula: Formula) {
        switch Formulas.sustituir(formula, incognita: incognita, valores: valores) {
        case .lista(let expresion, _):
            alEnviar(expresion)
            cerrar()
        case .faltan(let variables):
            let nombres = variables.map(\.simbolo).joined(separator: ", ")
            aviso = variables.count == 1
                ? "Falta \(nombres)."
                : "Faltan \(nombres)."
        case .noDespejable(let simbolo):
            aviso = "De esta fórmula no se puede despejar \(simbolo)."
        }
    }
}
