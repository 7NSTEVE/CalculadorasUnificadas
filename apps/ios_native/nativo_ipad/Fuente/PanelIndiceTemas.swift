import SwiftUI

struct PanelIndiceTemas: View {
    let temas: [TemaIndice]
    var alSeleccionarTema: (TemaIndice) -> Void
    var alCerrar: () -> Void
    
    var body: some View {
        NavigationView {
            List {
                if temas.isEmpty {
                    Section {
                        Text("No se encontraron temas.")
                            .foregroundColor(.secondary)
                            .italic()
                    } footer: {
                        Text("Escribe un texto grande (título) en tu lienzo para que aparezca aquí automáticamente.")
                    }
                } else {
                    ForEach(temas) { tema in
                        Button(action: {
                            alSeleccionarTema(tema)
                            alCerrar()
                        }) {
                            HStack {
                                Text(tema.titulo)
                                    .font(.body)
                                    .foregroundColor(.primary)
                                Spacer()
                                Text("\(tema.pagina)")
                                    .font(.subheadline)
                                    .foregroundColor(.secondary)
                                    .padding(.horizontal, 8)
                                    .padding(.vertical, 2)
                                    .background(Color.secondary.opacity(0.15))
                                    .cornerRadius(6)
                            }
                        }
                        .listRowBackground(Color.clear)
                    }
                }
            }
            .listStyle(.insetGrouped)
            .navigationTitle("Índice")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button(action: alCerrar) {
                        Image(systemName: "xmark")
                            .foregroundColor(.primary)
                    }
                }
            }
        }
        .preferredColorScheme(.light) // O heredar del entorno
    }
}
