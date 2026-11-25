import SwiftUI

struct SelectorColorApp: View {
    @Binding var colorSeleccionado: UInt32
    var alCerrar: () -> Void
    
    @State private var pestanaActiva = 0
    @State private var rellenar = false
    @State private var opacidad: Double = 1.0
    
    // Paleta aproximada de Formato Paginado
    let colores: [[UInt32]] = [
        [0xFFFFC0CB, 0xFFFFE4C4, 0xFFFFFACD, 0xFFE0FFFF, 0xFFE0FFFF, 0xFFE6E6FA, 0xFFE6E6FA, 0xFFFFF0F5],
        [0xFFFF0000, 0xFFFFA500, 0xFFFFFF00, 0xFF008000, 0xFF00BFFF, 0xFF0000FF, 0xFF8A2BE2, 0xFFFF1493],
        [0xFF8B0000, 0xFF8B4513, 0xFFB8860B, 0xFF006400, 0xFF008B8B, 0xFF00008B, 0xFF4B0082, 0xFF800080],
        [0xFFFFFFFF, 0xFFD3D3D3, 0xFFA9A9A9, 0xFF696969] // Última fila incompleta en preset
    ]
    
    var body: some View {
        VStack(spacing: 0) {
            // Encabezado
            HStack {
                Button(action: {}) {
                    Image(systemName: "slider.horizontal.3")
                        .font(.system(size: 16))
                        .foregroundColor(.white)
                        .frame(width: 36, height: 36)
                        .background(Color(hex: "#2c2c2e"))
                        .clipShape(Circle())
                }
                
                Spacer()
                
                Text("Color")
                    .font(.system(size: 17, weight: .semibold))
                    .foregroundColor(.white)
                
                Spacer()
                
                Button(action: {}) {
                    Text("Edit")
                        .font(.system(size: 15))
                        .foregroundColor(.white)
                        .padding(.horizontal, 12)
                        .padding(.vertical, 6)
                        .background(Color(hex: "#2c2c2e"))
                        .clipShape(Capsule())
                }
            }
            .padding(.horizontal, 16)
            .padding(.top, 16)
            .padding(.bottom, 12)
            
            // Segmented Control
            HStack(spacing: 0) {
                BotonPestanaGN(titulo: "Presets", seleccionado: pestanaActiva == 0) { pestanaActiva = 0 }
                BotonPestanaGN(titulo: "Custom", seleccionado: pestanaActiva == 1) { pestanaActiva = 1 }
                BotonPestanaGN(titulo: "History", seleccionado: pestanaActiva == 2) { pestanaActiva = 2 }
            }
            .background(Color(hex: "#2c2c2e"))
            .clipShape(Capsule())
            .padding(.horizontal, 16)
            .padding(.bottom, 24)
            
            // Cuadrícula de colores
            VStack(spacing: 16) {
                ForEach(0..<4, id: \.self) { fila in
                    HStack(spacing: 16) {
                        ForEach(0..<8, id: \.self) { col in
                            if fila < colores.count && col < colores[fila].count {
                                let c = colores[fila][col]
                                Circle()
                                    .fill(Color(UIColor(argb: c)))
                                    .frame(width: 24, height: 24)
                                    .overlay(
                                        Image(systemName: "checkmark")
                                            .font(.system(size: 10, weight: .bold))
                                            .foregroundColor(c == 0xFFFFFFFF ? .black : .white)
                                            .opacity(colorSeleccionado == c ? 1 : 0)
                                    )
                                    .onTapGesture {
                                        colorSeleccionado = c
                                    }
                            } else if fila == 3 && col == 4 {
                                // Botón Agregar
                                Circle()
                                    .strokeBorder(Color.gray, style: StrokeStyle(lineWidth: 1, dash: [4]))
                                    .frame(width: 24, height: 24)
                                    .overlay(
                                        Image(systemName: "plus")
                                            .font(.system(size: 12))
                                            .foregroundColor(.gray)
                                    )
                            } else {
                                Spacer().frame(width: 24, height: 24)
                            }
                        }
                    }
                }
            }
            .padding(.horizontal, 24)
            .padding(.bottom, 24)
            
            Divider().background(Color(hex: "#38383a"))
            
            // Opciones de Relleno y Opacidad
            VStack(spacing: 20) {
                Toggle("Fill Color", isOn: $rellenar)
                    .tint(Color(hex: "#34c759"))
                    .font(.system(size: 16))
                
                VStack(spacing: 8) {
                    HStack {
                        Text("Opacity")
                            .font(.system(size: 16))
                        Spacer()
                        Text("\(Int(opacidad * 100))%")
                            .font(.system(size: 14))
                            .foregroundColor(.gray)
                    }
                    
                    Slider(value: $opacidad, in: 0...1)
                        .tint(.white)
                }
            }
            .padding(20)
        }
        .frame(width: 320)
        .background(Color(hex: "#1c1c1e"))
        .cornerRadius(24)
        .preferredColorScheme(.dark)
    }
}

struct BotonPestanaGN: View {
    let titulo: String
    let seleccionado: Bool
    let accion: () -> Void
    
    var body: some View {
        Button(action: accion) {
            Text(titulo)
                .font(.system(size: 13, weight: .medium))
                .foregroundColor(seleccionado ? .white : .gray)
                .frame(maxWidth: .infinity)
                .padding(.vertical, 8)
                .background(seleccionado ? Color(hex: "#48484a") : Color.clear)
                .clipShape(Capsule())
        }
    }
}
