import SwiftUI

/// Selector unificado de las 4 calculadoras Casio nativas v47 dentro de CuadernU
/// (fx-CG50, fx-991CW, fx-570LA X, fx-570VN PLUS).
struct VistaSelector: View {
    var centro: CentroCasioCuadernU?

    init(centro: CentroCasioCuadernU? = nil) {
        self.centro = centro
    }

    var body: some View {
        if let centro, let activo = centro.modeloActivo, centro.modoSoloCalculadora {
            VStack(spacing: 0) {
                Picker("Calculadora", selection: Binding(
                    get: { centro.modeloActivo ?? ModeloCalculadora.cg50 },
                    set: { centro.modeloActivo = $0 }
                )) {
                    ForEach(ModeloCalculadora.todos) { m in
                        Text(m.nombre).tag(m)
                    }
                }
                .pickerStyle(.segmented)
                .padding(.horizontal, 10)
                .padding(.vertical, 6)

                vistaPara(activo)
                    .id(activo.id)
            }
        } else {
            NavigationStack {
                ScrollView {
                    VStack(spacing: 16) {
                        ForEach(ModeloCalculadora.todos) { modelo in
                            NavigationLink(value: modelo) {
                                TarjetaCalculadora(modelo: modelo)
                            }
                            .buttonStyle(.plain)
                        }
                    }
                    .padding(18)
                }
                .background(Color(.systemGroupedBackground).ignoresSafeArea())
                .navigationTitle("Calculadoras")
                .navigationDestination(for: ModeloCalculadora.self) { modelo in
                    vistaPara(modelo)
                }
            }
        }
    }

    @ViewBuilder
    private func vistaPara(_ modelo: ModeloCalculadora) -> some View {
        switch modelo.motor {
        case .cg50, .cw2:
            VistaCalculadoraV47(modelo: modelo, centro: centro)
        case .nx:
            VistaCalculadora(modelo: modelo)
        }
    }
}

private struct TarjetaCalculadora: View {
    let modelo: ModeloCalculadora

    var body: some View {
        HStack(spacing: 16) {
            if let imagen = UIImage.caratula(modelo.disposicion.caratula) {
                Image(uiImage: imagen)
                    .resizable()
                    .aspectRatio(contentMode: .fit)
                    .frame(height: 132)
                    .clipShape(RoundedRectangle(cornerRadius: 10))
            }

            VStack(alignment: .leading, spacing: 4) {
                Text(modelo.nombre)
                    .font(.headline)
                    .foregroundStyle(.primary)
                Text(modelo.descripcion)
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
            }
            Spacer()
            Image(systemName: "chevron.right")
                .foregroundStyle(.tertiary)
        }
        .padding(14)
        .background(Color(.secondarySystemGroupedBackground),
                    in: RoundedRectangle(cornerRadius: 14, style: .continuous))
    }
}

extension UIImage {
    /// Carga la carátula oficial sin compresión ni pérdida de escala.
    static func caratula(_ nombre: String) -> UIImage? {
        if let imagen = UIImage(named: nombre) { return imagen }
        guard let datos = Paquete.datos(nombre, extension: "png") else { return nil }
        return UIImage(data: datos)
    }
}
