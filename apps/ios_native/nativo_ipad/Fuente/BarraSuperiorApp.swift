import SwiftUI
import UIKit

// Extensión para colores hex en SwiftUI
extension Color {
    init(hex: String) {
        let hex = hex.trimmingCharacters(in: CharacterSet.alphanumerics.inverted)
        var int: UInt64 = 0
        Scanner(string: hex).scanHexInt64(&int)
        let a, r, g, b: UInt64
        switch hex.count {
        case 3:
            (a, r, g, b) = (255, (int >> 8) * 17, (int >> 4 & 0xF) * 17, (int & 0xF) * 17)
        case 6:
            (a, r, g, b) = (255, int >> 16, int >> 8 & 0xFF, int & 0xFF)
        case 8:
            (a, r, g, b) = (int >> 24, int >> 16, int >> 8 & 0xFF, int & 0xFF)
        default:
            (a, r, g, b) = (1, 1, 1, 0)
        }
        self.init(
            .sRGB,
            red: Double(r) / 255,
            green: Double(g) / 255,
            blue: Double(b) / 255,
            opacity: Double(a) / 255
        )
    }
}

/// Barra canónica del editor. Los accesos opcionales nunca fingen funcionar:
/// si no existe callback ni fallback real aparecen deshabilitados y VoiceOver
/// los anuncia como tales en lugar de aceptar un toque sin efecto.
struct BarraSuperiorApp: View {
    @ObservedObject var estado: EstadoBarraFlotante
    let colores: [UInt32]

    var alTocarDeshacer: () -> Void
    var alTocarLimpiar: () -> Void
    var alAlternarCalculadora: () -> Void
    var alTocarIndice: () -> Void
    var alCerrar: () -> Void

    var titulo: String
    var alTocarRehacer: (() -> Void)?
    var alCompartir: (() -> Void)?
    var alAbrirMas: (() -> Void)?
    var alTocarMarcador: (() -> Void)?
    var alInsertarTexto: (() -> Void)?
    var alInsertarFoto: (() -> Void)?
    var alAbrirStickers: (() -> Void)?

    @State private var popoverColorAbierto = false
    @State private var popoverBorradorAbierto = false

    private let colorFondoNav = Color(hex: "#1c2841")
    private let colorFondoTool = Color(hex: "#1e1e1e")

    init(
        estado: EstadoBarraFlotante,
        colores: [UInt32],
        alTocarDeshacer: @escaping () -> Void,
        alTocarLimpiar: @escaping () -> Void,
        alAlternarCalculadora: @escaping () -> Void,
        alTocarIndice: @escaping () -> Void,
        alCerrar: @escaping () -> Void,
        titulo: String = "CuadernU",
        alTocarRehacer: (() -> Void)? = nil,
        alCompartir: (() -> Void)? = nil,
        alAbrirMas: (() -> Void)? = nil,
        alTocarMarcador: (() -> Void)? = nil,
        alInsertarTexto: (() -> Void)? = nil,
        alInsertarFoto: (() -> Void)? = nil,
        alAbrirStickers: (() -> Void)? = nil
    ) {
        self.estado = estado
        self.colores = colores
        self.alTocarDeshacer = alTocarDeshacer
        self.alTocarLimpiar = alTocarLimpiar
        self.alAlternarCalculadora = alAlternarCalculadora
        self.alTocarIndice = alTocarIndice
        self.alCerrar = alCerrar
        self.titulo = titulo.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
            ? "CuadernU" : titulo
        self.alTocarRehacer = alTocarRehacer ?? {
            Task { @MainActor in
                CentroAccionesToolbarDocumentoIPad.compartido.rehacer()
            }
        }
        self.alCompartir = alCompartir
        self.alAbrirMas = alAbrirMas
        self.alTocarMarcador = alTocarMarcador
        self.alInsertarTexto = alInsertarTexto ?? {
            Task { @MainActor in
                CentroAccionesToolbarDocumentoIPad.compartido.insertarTexto()
            }
        }
        self.alInsertarFoto = alInsertarFoto ?? {
            Task { @MainActor in
                CentroAccionesToolbarDocumentoIPad.compartido.insertarFoto()
            }
        }
        self.alAbrirStickers = alAbrirStickers
    }

    var body: some View {
        VStack(spacing: 0) {
            barraDocumento
            barraHerramientas
            Rectangle()
                .fill(Color.black.opacity(0.3))
                .frame(height: 1)
        }
        .preferredColorScheme(.dark)
    }

    private var barraDocumento: some View {
        HStack(spacing: 8) {
            BotonAccionBarraSuperior(
                icono: "sidebar.left",
                etiqueta: "Mostrar índice",
                accion: alTocarIndice)

            BotonAccionBarraSuperior(
                icono: "chevron.left",
                etiqueta: "Cerrar documento",
                accion: alCerrar)

            HStack(spacing: 8) {
                Text(titulo)
                    .font(.system(.subheadline, design: .rounded, weight: .medium))
                    .foregroundColor(.white)
                    .lineLimit(1)
                    .truncationMode(.middle)
                    .accessibilityAddTraits(.isHeader)
                Image(systemName: "doc.text")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            .padding(.horizontal, 12)
            .frame(minHeight: 36)
            .background(Color.white.opacity(0.09), in: RoundedRectangle(cornerRadius: 9))
            .frame(maxWidth: 420, alignment: .leading)

            Spacer(minLength: 8)

            BotonAccionBarraSuperior(
                icono: "square.and.arrow.up",
                etiqueta: "Compartir",
                accionOpcional: alCompartir)
            BotonAccionBarraSuperior(
                icono: "ellipsis.circle",
                etiqueta: "Más opciones",
                accionOpcional: alAbrirMas)
        }
        .padding(.horizontal, 12)
        .padding(.top, 40)
        .padding(.bottom, 8)
        .frame(minHeight: 90, alignment: .bottom)
        .background(colorFondoNav.ignoresSafeArea(edges: .top))
    }

    private var barraHerramientas: some View {
        ViewThatFits(in: .horizontal) {
            herramientasCompletas
            ScrollView(.horizontal, showsIndicators: false) {
                herramientasCompletas
                    .padding(.horizontal, 4)
            }
        }
        .frame(height: 56)
        .background(colorFondoTool)
    }

    private var herramientasCompletas: some View {
        HStack(spacing: 8) {
            grupoNavegacion
            separador
            grupoEdicion
            separador
            grupoInsercion
            Spacer(minLength: 8)
            grupoContextual
            separador
            grupoHistorial
        }
        .padding(.horizontal, 12)
        .frame(minWidth: 760, minHeight: 54)
    }

    private var grupoNavegacion: some View {
        HStack(spacing: 4) {
            BotonAccionBarraSuperior(
                icono: "square.grid.2x2",
                etiqueta: "Páginas e índice",
                accion: alTocarIndice)

            Button {
                Task { @MainActor in
                    CentroAccionesToolbarDocumentoIPad.compartido.abrirBusquedaUniversal()
                }
            } label: {
                Image(systemName: "magnifyingglass")
                    .frame(width: 44, height: 44)
            }
            .foregroundColor(.white)
            .contentShape(Rectangle())
            .accessibilityLabel("Buscar en CuadernU")
            .accessibilityHint("Busca en tinta, OCR, PDF, texto, audio, problemas, fórmulas y fuentes")
            .accessibilityIdentifier("barra.buscar.universal")

            BotonAccionBarraSuperior(
                icono: "bookmark",
                etiqueta: "Marcador",
                accionOpcional: alTocarMarcador)
        }
    }

    private var grupoEdicion: some View {
        HStack(spacing: 4) {
            BotonHerramientaGN(
                icono: "lasso",
                etiqueta: "Lazo",
                activo: estado.herramientaActiva == .lazo) {
                    estado.herramientaActiva = .lazo
                }

            BotonHerramientaGN(
                icono: "pencil.tip",
                etiqueta: "Pluma",
                activo: estado.herramientaActiva == .pluma) {
                    estado.herramientaActiva = .pluma
                }

            BotonHerramientaGN(
                icono: "eraser",
                etiqueta: "Borrador",
                activo: estado.herramientaActiva == .borradorObjeto) {
                    if estado.herramientaActiva == .borradorObjeto {
                        popoverBorradorAbierto.toggle()
                    } else {
                        estado.herramientaActiva = .borradorObjeto
                    }
                }
                .popover(isPresented: $popoverBorradorAbierto) {
                    PopoverBorradorApp(alCerrar: { popoverBorradorAbierto = false })
                }

            BotonHerramientaGN(
                icono: "highlighter",
                etiqueta: "Resaltador",
                activo: estado.herramientaActiva == .resaltador) {
                    estado.herramientaActiva = .resaltador
                }
        }
    }

    private var grupoInsercion: some View {
        HStack(spacing: 4) {
            BotonAccionBarraSuperior(
                icono: "character.textbox",
                etiqueta: "Insertar texto",
                accionOpcional: alInsertarTexto)
            BotonAccionBarraSuperior(
                icono: "photo",
                etiqueta: "Insertar foto",
                accionOpcional: alInsertarFoto)
            BotonAccionBarraSuperior(
                icono: "sticker",
                etiqueta: "Stickers",
                accionOpcional: alAbrirStickers)
        }
    }

    @ViewBuilder
    private var grupoContextual: some View {
        HStack(spacing: 8) {
            Button(action: alAlternarCalculadora) {
                Image(systemName: "function")
                    .font(.system(size: 18, weight: .semibold))
                    .foregroundColor(.white)
                    .frame(width: 40, height: 40)
                    .background(Color.blue, in: Circle())
            }
            .accessibilityLabel("Calculadora")
            .accessibilityHint("Muestra u oculta la calculadora integrada")

            if estado.herramientaActiva == .pluma ||
                estado.herramientaActiva == .resaltador {
                HStack(spacing: 8) {
                    ForEach(Array(colores.prefix(3).enumerated()), id: \.offset) { indice, colorVal in
                        Button {
                            if estado.colorActivo == colorVal {
                                popoverColorAbierto.toggle()
                            } else {
                                estado.colorActivo = colorVal
                            }
                        } label: {
                            Circle()
                                .fill(Color(UIColor(argb: colorVal)))
                                .frame(width: 26, height: 26)
                                .overlay(
                                    Circle().strokeBorder(
                                        Color.white,
                                        lineWidth: estado.colorActivo == colorVal ? 2 : 0)
                                )
                                .frame(width: 44, height: 44)
                        }
                        .buttonStyle(.plain)
                        .accessibilityLabel("Color \(indice + 1)")
                        .accessibilityValue(
                            estado.colorActivo == colorVal ? "Seleccionado" : "No seleccionado")
                    }
                }
                .popover(isPresented: $popoverColorAbierto) {
                    SelectorColorApp(
                        colorSeleccionado: $estado.colorActivo,
                        alCerrar: { popoverColorAbierto = false })
                }
            }
        }
    }

    private var grupoHistorial: some View {
        HStack(spacing: 4) {
            BotonAccionBarraSuperior(
                icono: "arrow.uturn.backward",
                etiqueta: "Deshacer",
                accion: alTocarDeshacer)
            BotonAccionBarraSuperior(
                icono: "arrow.uturn.forward",
                etiqueta: "Rehacer",
                accionOpcional: alTocarRehacer)
            Button(action: alTocarLimpiar) {
                Image(systemName: "trash")
                    .frame(width: 44, height: 44)
            }
            .foregroundColor(.red)
            .contentShape(Rectangle())
            .accessibilityLabel("Limpiar lienzo")
        }
    }

    private var separador: some View {
        Divider()
            .overlay(Color.white.opacity(0.15))
            .frame(height: 28)
            .padding(.horizontal, 2)
    }
}

private struct BotonAccionBarraSuperior: View {
    let icono: String
    let etiqueta: String
    let accion: (() -> Void)?

    init(icono: String, etiqueta: String, accion: @escaping () -> Void) {
        self.icono = icono
        self.etiqueta = etiqueta
        self.accion = accion
    }

    init(icono: String, etiqueta: String, accionOpcional: (() -> Void)?) {
        self.icono = icono
        self.etiqueta = etiqueta
        self.accion = accionOpcional
    }

    var body: some View {
        Button {
            accion?()
        } label: {
            Image(systemName: icono)
                .font(.system(size: 18))
                .frame(width: 44, height: 44)
        }
        .foregroundColor(.white)
        .contentShape(Rectangle())
        .disabled(accion == nil)
        .opacity(accion == nil ? 0.35 : 1)
        .accessibilityLabel(etiqueta)
        .accessibilityHint(accion == nil ? "Disponible cuando esta función esté conectada" : "")
    }
}

struct BotonHerramientaGN: View {
    let icono: String
    let etiqueta: String
    let activo: Bool
    let accion: () -> Void

    init(icono: String,
         etiqueta: String? = nil,
         activo: Bool,
         accion: @escaping () -> Void) {
        self.icono = icono
        self.etiqueta = etiqueta ?? icono
        self.activo = activo
        self.accion = accion
    }

    var body: some View {
        Button(action: accion) {
            Image(systemName: icono)
                .font(.system(size: 20, weight: activo ? .semibold : .regular))
                .foregroundColor(activo ? Color(hex: "#64d2ff") : .white)
                .frame(width: 44, height: 44)
                .background(
                    RoundedRectangle(cornerRadius: 8)
                        .fill(activo ? Color(hex: "#64d2ff").opacity(0.2) : Color.clear)
                )
        }
        .buttonStyle(.plain)
        .contentShape(Rectangle())
        .accessibilityLabel(etiqueta)
        .accessibilityValue(activo ? "Seleccionado" : "")
        .accessibilityAddTraits(activo ? .isSelected : [])
    }
}

struct RoundedCorner: Shape {
    var radius: CGFloat = .infinity
    var corners: UIRectCorner = .allCorners

    func path(in rect: CGRect) -> Path {
        let path = UIBezierPath(
            roundedRect: rect,
            byRoundingCorners: corners,
            cornerRadii: CGSize(width: radius, height: radius))
        return Path(path.cgPath)
    }
}

extension View {
    func cornerRadius(_ radius: CGFloat, corners: UIRectCorner) -> some View {
        clipShape(RoundedCorner(radius: radius, corners: corners))
    }
}
