import AppIntents
import SwiftUI
import WidgetKit

struct NuevaNotaWidgetTintaIntent: AppIntent {
    static var title: LocalizedStringResource = "Nueva nota en Tinta"
    static var openAppWhenRun = true
    func perform() async throws -> some IntentResult {
        try BuzonCompartidoSistemaIPad.publicar(
            EntradaSistemaIPad(tipo: .nuevaNota))
        return .result()
    }
}

struct FotoWidgetTintaIntent: AppIntent {
    static var title: LocalizedStringResource = "Capturar pizarra en Tinta"
    static var openAppWhenRun = true
    func perform() async throws -> some IntentResult {
        try BuzonCompartidoSistemaIPad.publicar(
            EntradaSistemaIPad(tipo: .capturarFoto))
        return .result()
    }
}

struct VozWidgetTintaIntent: AppIntent {
    static var title: LocalizedStringResource = "Dictar en Tinta"
    static var openAppWhenRun = true
    func perform() async throws -> some IntentResult {
        try BuzonCompartidoSistemaIPad.publicar(
            EntradaSistemaIPad(tipo: .capturarVoz))
        return .result()
    }
}

struct EntradaTimelineTintaIPad: TimelineEntry {
    var date: Date
    var estado: EstadoWidgetTintaIPad
}

struct ProveedorTimelineTintaIPad: TimelineProvider {
    func placeholder(in context: Context) -> EntradaTimelineTintaIPad {
        EntradaTimelineTintaIPad(date: Date(), estado: EstadoWidgetTintaIPad())
    }
    func getSnapshot(in context: Context,
                     completion: @escaping (EntradaTimelineTintaIPad) -> Void) {
        completion(EntradaTimelineTintaIPad(date: Date(),
                                            estado: BuzonCompartidoSistemaIPad.estadoWidget()))
    }
    func getTimeline(in context: Context,
                     completion: @escaping (Timeline<EntradaTimelineTintaIPad>) -> Void) {
        let entrada = EntradaTimelineTintaIPad(
            date: Date(), estado: BuzonCompartidoSistemaIPad.estadoWidget())
        completion(Timeline(entries: [entrada], policy: .after(
            Date().addingTimeInterval(15 * 60))))
    }
}

struct VistaWidgetTintaIPad: View {
    @Environment(\.widgetFamily) private var familia
    let entry: EntradaTimelineTintaIPad
    var body: some View {
        VStack(alignment: .leading, spacing: 5) {
            Link(destination: URL(string: "tinta://continuar")!) {
                Label("Tinta", systemImage: "pencil.and.outline")
                    .font(.headline)
            }
            if familia == .accessoryInline {
                Link(entry.estado.accion,
                     destination: URL(string: "tinta://continuar")!)
            } else if familia == .accessoryCircular {
                Button(intent: NuevaNotaWidgetTintaIntent()) {
                    Image(systemName: "square.and.pencil")
                }.buttonStyle(.plain)
            } else {
                Link(destination: URL(string: "tinta://continuar")!) {
                    Text(entry.estado.proximaClase).font(.caption)
                    Text(entry.estado.continuar).font(.subheadline).bold()
                }
                if familia == .systemMedium {
                    Text(entry.estado.proximaEvaluacion).font(.caption)
                    HStack {
                        Button(intent: NuevaNotaWidgetTintaIntent()) {
                            Label("Nota", systemImage: "square.and.pencil")
                        }
                        Button(intent: FotoWidgetTintaIntent()) {
                            Label("Foto", systemImage: "camera")
                        }
                        Button(intent: VozWidgetTintaIntent()) {
                            Label("Voz", systemImage: "mic")
                        }
                    }.font(.caption2).buttonStyle(.plain)
                }
            }
        }
        .containerBackground(.fill.tertiary, for: .widget)
    }
}

struct WidgetContinuarTintaIPad: Widget {
    let kind = "WidgetContinuarTintaIPad"
    var body: some WidgetConfiguration {
        StaticConfiguration(kind: kind, provider: ProveedorTimelineTintaIPad()) {
            VistaWidgetTintaIPad(entry: $0)
        }
        .configurationDisplayName("Continuar Tinta")
        .description("Próxima clase, continuación y evaluación sin abrir Home.")
        .supportedFamilies([.systemSmall, .systemMedium, .accessoryRectangular,
                            .accessoryInline, .accessoryCircular])
    }
}

private struct VistaCapturaWidgetTintaIPad<Intento: AppIntent>: View {
    let titulo: String
    let simbolo: String
    let intento: Intento
    var body: some View {
        Button(intent: intento) {
            VStack(spacing: 5) {
                Image(systemName: simbolo).font(.title2)
                Text(titulo).font(.caption2).lineLimit(1)
            }
        }
        .buttonStyle(.plain)
        .containerBackground(.fill.tertiary, for: .widget)
    }
}

struct WidgetPizarraTintaIPad: Widget {
    let kind = "WidgetPizarraTintaIPad"
    var body: some WidgetConfiguration {
        StaticConfiguration(kind: kind, provider: ProveedorTimelineTintaIPad()) { _ in
            VistaCapturaWidgetTintaIPad(
                titulo: "Pizarra", simbolo: "camera.viewfinder",
                intento: FotoWidgetTintaIntent())
        }
        .configurationDisplayName("Capturar pizarra")
        .description("Abre el escáner de Tinta desde Inicio o la pantalla bloqueada.")
        .supportedFamilies([.systemSmall, .accessoryCircular])
    }
}

struct WidgetVozTintaIPad: Widget {
    let kind = "WidgetVozTintaIPad"
    var body: some WidgetConfiguration {
        StaticConfiguration(kind: kind, provider: ProveedorTimelineTintaIPad()) { _ in
            VistaCapturaWidgetTintaIPad(
                titulo: "Voz", simbolo: "mic",
                intento: VozWidgetTintaIntent())
        }
        .configurationDisplayName("Capturar voz")
        .description("Abre el dictado local de Tinta desde Inicio o la pantalla bloqueada.")
        .supportedFamilies([.systemSmall, .accessoryCircular])
    }
}

@main
struct WidgetsTintaIPad: WidgetBundle {
    var body: some Widget {
        WidgetContinuarTintaIPad()
        WidgetPizarraTintaIPad()
        WidgetVozTintaIPad()
    }
}
