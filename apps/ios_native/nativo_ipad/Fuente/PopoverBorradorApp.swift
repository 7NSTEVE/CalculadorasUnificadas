import SwiftUI

struct PopoverBorradorApp: View {
    var alCerrar: () -> Void
    
    // Estados ficticios para los toggles (solo visual por ahora)
    @State private var borrarPen = true
    @State private var borrarPencil = true
    @State private var borrarHighlighter = true
    @State private var borrarTape = true
    @State private var borrarText = true
    @State private var borrarElements = true
    @State private var borrarImages = true
    @State private var borrarStickyNotes = true
    
    var todosSeleccionados: Bool {
        borrarPen && borrarPencil && borrarHighlighter && borrarTape &&
        borrarText && borrarElements && borrarImages && borrarStickyNotes
    }
    
    var body: some View {
        VStack(spacing: 0) {
            // Encabezado
            HStack {
                Button(action: alCerrar) {
                    Image(systemName: "chevron.left")
                        .font(.system(size: 16, weight: .semibold))
                        .foregroundColor(.white)
                        .frame(width: 36, height: 36)
                        .background(Color(hex: "#2c2c2e"))
                        .clipShape(Circle())
                }
                
                Spacer()
                
                Text("Delete Specific Items")
                    .font(.system(size: 17, weight: .semibold))
                    .foregroundColor(.white)
                
                Spacer()
                
                Button(action: {
                    let estado = !todosSeleccionados
                    borrarPen = estado
                    borrarPencil = estado
                    borrarHighlighter = estado
                    borrarTape = estado
                    borrarText = estado
                    borrarElements = estado
                    borrarImages = estado
                    borrarStickyNotes = estado
                }) {
                    Text(todosSeleccionados ? "Deselect all" : "Select all")
                        .font(.system(size: 14))
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
            
            // Subheader
            HStack {
                Text("TOOL")
                    .font(.system(size: 12, weight: .medium))
                    .foregroundColor(.gray)
                Spacer()
            }
            .padding(.horizontal, 16)
            .padding(.bottom, 8)
            
            // Lista de Toggles
            ScrollView {
                VStack(spacing: 0) {
                    FilaToggleGN(icono: "pencil.tip", titulo: "Pen", isOn: $borrarPen)
                    FilaToggleGN(icono: "pencil", titulo: "Pencil", isOn: $borrarPencil)
                    FilaToggleGN(icono: "highlighter", titulo: "Highlighter", isOn: $borrarHighlighter)
                    FilaToggleGN(icono: "bandage", titulo: "Tape", isOn: $borrarTape)
                    FilaToggleGN(icono: "character.textbox", titulo: "Text", isOn: $borrarText)
                    FilaToggleGN(icono: "moon", titulo: "Elements", isOn: $borrarElements) // Icono aproximado
                    FilaToggleGN(icono: "photo", titulo: "Images", isOn: $borrarImages)
                    FilaToggleGN(icono: "note.text", titulo: "Sticky Notes", isOn: $borrarStickyNotes)
                }
                .background(Color(hex: "#2c2c2e"))
                .cornerRadius(12)
                .padding(.horizontal, 16)
            }
            .frame(maxHeight: 350)
            
            // Botón Inferior
            Button(action: {}) {
                Text("Delete Page Items")
                    .font(.system(size: 16, weight: .semibold))
                    .foregroundColor(Color.gray.opacity(0.5))
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 14)
                    .background(Color(hex: "#2c2c2e"))
                    .clipShape(Capsule())
            }
            .disabled(true)
            .padding(16)
        }
        .frame(width: 340)
        .background(Color(hex: "#1c1c1e"))
        .cornerRadius(24)
        .preferredColorScheme(.dark)
    }
}

struct FilaToggleGN: View {
    let icono: String
    let titulo: String
    @Binding var isOn: Bool
    
    var body: some View {
        HStack(spacing: 16) {
            Image(systemName: icono)
                .font(.system(size: 18))
                .foregroundColor(.white)
                .frame(width: 24, alignment: .center)
            
            Text(titulo)
                .font(.system(size: 16))
                .foregroundColor(.white)
            
            Spacer()
            
            Toggle("", isOn: $isOn)
                .tint(Color(hex: "#34c759"))
                .labelsHidden()
        }
        .padding(.horizontal, 16)
        .padding(.vertical, 10)
        Divider().background(Color(hex: "#38383a")).padding(.leading, 56)
    }
}
