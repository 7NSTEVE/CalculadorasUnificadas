import SwiftUI
import Combine

class EstadoBarraFlotante: ObservableObject {
    @Published var herramientaActiva: HerramientaLienzo
    @Published var colorActivo: UInt32
    
    init(herramienta: HerramientaLienzo, color: UInt32) {
        self.herramientaActiva = herramienta
        self.colorActivo = color
    }
}

struct BarraHerramientasFlotante: View {
    @ObservedObject var estado: EstadoBarraFlotante
    
    let colores: [UInt32]
    
    var alTocarDeshacer: () -> Void
    var alTocarLimpiar: () -> Void
    var alAlternarCalculadora: () -> Void
    var alTocarIndice: () -> Void
    
    var body: some View {
        HStack(spacing: 16) {
            
            // Botón de Índice
            Button(action: alTocarIndice) {
                Image(systemName: "list.bullet.rectangle.portrait")
                    .font(.system(size: 22, weight: .medium))
                    .foregroundColor(.primary)
            }
            
            Divider().frame(height: 30)
            // Herramientas de trazo
            ForEach([HerramientaLienzo.pluma, .resaltador, .borradorObjeto, .lazo], id: \.self) { herramienta in
                BotonHerramienta(
                    icono: herramienta.simbolo,
                    activo: estado.herramientaActiva == herramienta,
                    accion: { estado.herramientaActiva = herramienta }
                )
            }
            
            Divider()
                .frame(height: 24)
            
            // Selector de colores
            HStack(spacing: 8) {
                ForEach(colores, id: \.self) { color in
                    BotonColor(
                        color: color,
                        activo: estado.colorActivo == color,
                        accion: { 
                            estado.colorActivo = color
                            // Si toca un color, cambiar automáticamente a pluma si estaba en borrador/lazo
                            if estado.herramientaActiva != .pluma && estado.herramientaActiva != .resaltador {
                                estado.herramientaActiva = .pluma
                            }
                        }
                    )
                }
            }
            
            Divider()
                .frame(height: 24)
            
            // Acciones globales
            Button(action: alTocarDeshacer) {
                Image(systemName: "arrow.uturn.backward.circle")
                    .font(.system(size: 24))
                    .foregroundColor(.primary)
            }
            Button(action: alTocarLimpiar) {
                Image(systemName: "trash.circle")
                    .font(.system(size: 24))
                    .foregroundColor(.red)
            }
            
            Button(action: alAlternarCalculadora) {
                Image(systemName: "function")
                    .font(.system(size: 22, weight: .semibold))
                    .foregroundColor(.white)
                    .frame(width: 36, height: 36)
                    .background(Color.blue)
                    .clipShape(Circle())
            }
        }
        .padding(.horizontal, 20)
        .padding(.vertical, 12)
        .background(
            Capsule()
                .fill(.ultraThinMaterial)
                .shadow(color: Color.black.opacity(0.15), radius: 10, x: 0, y: 5)
        )
    }
}

private struct BotonHerramienta: View {
    let icono: String
    let activo: Bool
    let accion: () -> Void
    
    var body: some View {
        Button(action: accion) {
            Image(systemName: icono)
                .font(.system(size: 20, weight: activo ? .semibold : .regular))
                .foregroundColor(activo ? .white : .primary)
                .frame(width: 44, height: 44)
                .background(
                    Circle()
                        .fill(activo ? Color.accentColor : Color.clear)
                )
        }
    }
}

private struct BotonColor: View {
    let color: UInt32
    let activo: Bool
    let accion: () -> Void
    
    var body: some View {
        Button(action: accion) {
            Circle()
                .fill(Color(UIColor(argb: color)))
                .frame(width: 28, height: 28)
                .overlay(
                    Circle()
                        .strokeBorder(Color.primary.opacity(0.3), lineWidth: 1)
                )
                .overlay(
                    Circle()
                        .strokeBorder(Color.white, lineWidth: activo ? 3 : 0)
                        .padding(2)
                )
                .shadow(color: activo ? Color(UIColor(argb: color)).opacity(0.5) : .clear, radius: 4)
        }
    }
}
