import SwiftUI

/// «¿Cómo hago esto en mi calculadora?»
///
/// Buscas «matriz» y te dice la ruta de la máquina que tienes delante, no la del
/// manual de otra. Y enseña al lado la de la otra, porque cuando has aprendido en
/// una y te examinas con la otra, lo que falla es justo eso.
struct VistaProcedimientos: View {
    let modelo: ModeloCalculadora
    /// Enciende las teclas de la ruta sobre la carátula, numeradas.
    let alSenalarEnTeclado: ([String]) -> Void
    /// Las pulsa de verdad.
    let alEjecutar: ([String]) -> Void

    @Environment(\.dismiss) private var cerrar
    @State private var busqueda = ""

    private var resultados: [Procedimiento] {
        ProcedimientosCasio.buscar(busqueda)
    }

    private var otroModelo: ModeloCalculadora? {
        ModeloCalculadora.todos.first { $0.id != modelo.id }
    }

    var body: some View {
        NavigationStack {
            List {
                if resultados.isEmpty {
                    Text("Nada con «\(busqueda)». Prueba con «matriz», «desviación», "
                         + "«complejo», «binario», «ecuación»…")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
                ForEach(resultados) { procedimiento in
                    Section(procedimiento.nombre) {
                        contenido(de: procedimiento)
                    }
                }
            }
            .listStyle(.insetGrouped)
            .searchable(text: $busqueda, prompt: "matriz, desviación estándar, binario…")
            .navigationTitle("Cómo se hace")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Cerrar") { cerrar() }
                }
            }
        }
    }

    @ViewBuilder private func contenido(de procedimiento: Procedimiento) -> some View {
        if let ruta = procedimiento.ruta(en: modelo.id) {
            VStack(alignment: .leading, spacing: 8) {
                Text(ruta.comoSeLee)
                    .font(.title3.weight(.semibold))
                Text("en tu \(modelo.nombre)")
                    .font(.caption)
                    .foregroundStyle(.secondary)

                if let despues = ruta.despues {
                    Label(despues, systemImage: "arrow.turn.down.right")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
            .padding(.vertical, 2)

            HStack(spacing: 16) {
                Button {
                    alSenalarEnTeclado(ruta.teclas)
                    cerrar()
                } label: {
                    Label("Enséñame dónde", systemImage: "hand.point.up.left")
                }
                Button {
                    alEjecutar(ruta.teclas)
                    cerrar()
                } label: {
                    Label("Hazlo tú", systemImage: "play")
                }
            }
            .font(.footnote.weight(.medium))
            .buttonStyle(.borderless)
        } else {
            Label("Tu \(modelo.nombre) no lo trae.", systemImage: "xmark.circle")
                .font(.callout)
                .foregroundStyle(.orange)
        }

        // El comparador entre modelos: la razón por la que uno se equivoca de tecla
        // al cambiar de calculadora.
        if let otro = otroModelo, let rutaOtro = procedimiento.ruta(en: otro.id) {
            HStack(alignment: .firstTextBaseline) {
                Text(otro.descripcion)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Spacer()
                Text(rutaOtro.comoSeLee)
                    .font(.footnote.weight(.medium))
                    .foregroundStyle(.secondary)
            }
        }
    }
}
