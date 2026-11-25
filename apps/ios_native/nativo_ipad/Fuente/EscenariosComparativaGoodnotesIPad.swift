import Foundation

enum CategoriaEscenarioGoodnotesIPad: String, Codable, CaseIterable {
    case pencil
    case hugeDocument
    case search
    case export
    case editing
}

struct EscenarioComparativaGoodnotesIPad: Codable, Identifiable, Equatable {
    var id: String
    var version: Int
    var categoria: CategoriaEscenarioGoodnotesIPad
    var nombre: String
    var metrica: String
    var unidad: String
    var direccion: DireccionMetricaComparativaIPad
    var minimoMuestras: Int
    var warmup: Int
    var requiere120Hz: Bool
    var requierePencilFisico: Bool
    var descripcionCorpus: String
    var pasos: [String]

    var idVersionado: String { "\(id)@v\(version)" }
}

enum CatalogoEscenariosGoodnotesIPad {
    static let version = 1

    static let pencilP95 = EscenarioComparativaGoodnotesIPad(
        id: "pencil-event-gpu-p95",
        version: 1,
        categoria: .pencil,
        nombre: "Pencil P95 event→GPU",
        metrica: "pencil_event_gpu_p95_ms",
        unidad: "ms",
        direccion: .menorEsMejor,
        minimoMuestras: 5,
        warmup: 2,
        requiere120Hz: true,
        requierePencilFisico: true,
        descripcionCorpus: "Página rayada 1240×1754, pluma 3 pt, 240 o más actualizaciones válidas por corrida.",
        pasos: [
            "Cerrar apps pesadas y esperar estado térmico nominal o fair.",
            "Abrir el mismo documento local sin colaboración ni grabación.",
            "Escribir durante al menos 10 s con trazos continuos y curvas cortas.",
            "Descartar las corridas warm-up; conservar P95 sólo si hay >=240 muestras.",
            "Repetir en CuadernU y Goodnotes en el mismo iPad, OS y refresco.",
        ])

    static let pencilP99 = EscenarioComparativaGoodnotesIPad(
        id: "pencil-event-gpu-p99",
        version: 1,
        categoria: .pencil,
        nombre: "Pencil P99 event→GPU",
        metrica: "pencil_event_gpu_p99_ms",
        unidad: "ms",
        direccion: .menorEsMejor,
        minimoMuestras: 5,
        warmup: 2,
        requiere120Hz: true,
        requierePencilFisico: true,
        descripcionCorpus: pencilP95.descripcionCorpus,
        pasos: pencilP95.pasos)

    static let huge500ScrollJank = EscenarioComparativaGoodnotesIPad(
        id: "huge-pdf-500-scroll-jank",
        version: 1,
        categoria: .hugeDocument,
        nombre: "Scroll 500 páginas PDF",
        metrica: "dropped_frames_pct",
        unidad: "%",
        direccion: .menorEsMejor,
        minimoMuestras: 5,
        warmup: 1,
        requiere120Hz: true,
        requierePencilFisico: false,
        descripcionCorpus: "Un único PDF local de 500 páginas, idéntico archivo SHA-256 en ambos productos, con anotaciones vectoriales predefinidas en cada 10 páginas.",
        pasos: [
            "Importar exactamente el mismo PDF y esperar que termine cualquier indexado inicial.",
            "Cerrar y reabrir la app antes de cada corrida medida.",
            "Recorrer de página 1 a 500 y volver a 1 con desplazamiento continuo durante 45–60 s.",
            "Registrar porcentaje de frames perdidos y pico de memoria del proceso.",
            "No comparar corridas con thermal serious/critical contra corridas nominales.",
        ])

    static let huge500Reopen = EscenarioComparativaGoodnotesIPad(
        id: "huge-pdf-500-reopen",
        version: 1,
        categoria: .hugeDocument,
        nombre: "Reapertura 500 páginas PDF",
        metrica: "reopen_to_first_usable_ms",
        unidad: "ms",
        direccion: .menorEsMejor,
        minimoMuestras: 7,
        warmup: 1,
        requiere120Hz: false,
        requierePencilFisico: false,
        descripcionCorpus: huge500ScrollJank.descripcionCorpus,
        pasos: [
            "Forzar cierre de la app después de confirmar persistencia del documento.",
            "Abrir directamente el mismo cuaderno desde biblioteca.",
            "Medir hasta que la página visible acepta scroll y una entrada de tinta sin placeholder bloqueante.",
            "Repetir siete veces alternando el orden CuadernU/Goodnotes para reducir sesgo de caché térmica.",
        ])

    static let searchTenThousand = EscenarioComparativaGoodnotesIPad(
        id: "search-10k-p95",
        version: 1,
        categoria: .search,
        nombre: "Búsqueda universal 10k",
        metrica: "search_p95_ms",
        unidad: "ms",
        direccion: .menorEsMejor,
        minimoMuestras: 10,
        warmup: 3,
        requiere120Hz: false,
        requierePencilFisico: false,
        descripcionCorpus: "10 000 objetos indexables con distribución fija de texto, handwriting OCR, títulos y PDF; mismas consultas y mismos aciertos esperados.",
        pasos: [
            "Esperar indexación completa en ambos productos.",
            "Ejecutar la lista canónica de consultas exactas y parciales en el mismo orden permutado por corrida.",
            "Medir desde submit hasta resultados utilizables y registrar P95.",
            "Validar además recall/precision del conjunto esperado; rapidez con resultados incorrectos no cuenta.",
        ])

    static let export500PDF = EscenarioComparativaGoodnotesIPad(
        id: "export-pdf-500-throughput",
        version: 1,
        categoria: .export,
        nombre: "Exportación PDF 500 páginas",
        metrica: "export_mb_s",
        unidad: "MB/s",
        direccion: .mayorEsMejor,
        minimoMuestras: 5,
        warmup: 1,
        requiere120Hz: false,
        requierePencilFisico: false,
        descripcionCorpus: "Mismo documento 500 páginas con PDF de fondo, tinta, texto e imágenes; exportar a PDF conservando fidelidad visible.",
        pasos: [
            "Usar almacenamiento local con espacio libre >5 GB.",
            "Exportar el documento completo sin compartir a red/nube.",
            "Medir bytes finales / duración y verificar que el PDF abra y tenga 500 páginas.",
            "Comparar fidelidad sobre un corpus fijo de páginas antes de aceptar la corrida.",
        ])

    static let lassoTenThousand = EscenarioComparativaGoodnotesIPad(
        id: "lasso-10k-transform",
        version: 1,
        categoria: .editing,
        nombre: "Lasso y transformación 10k trazos",
        metrica: "lasso_transform_p95_ms",
        unidad: "ms",
        direccion: .menorEsMejor,
        minimoMuestras: 7,
        warmup: 2,
        requiere120Hz: true,
        requierePencilFisico: false,
        descripcionCorpus: "Página con 10 000 trazos vectoriales distribuidos en 100 grupos; seleccionar 1 000 trazos y aplicar movimiento/escala reversible.",
        pasos: [
            "Abrir el corpus ya persistido y dejar estabilizar la vista.",
            "Ejecutar el mismo lazo predefinido.",
            "Medir selección+preview+commit y comprobar un solo Undo cuando la operación sea lógica única.",
            "Repetir con el mismo zoom y viewport.",
        ])

    static let todos: [EscenarioComparativaGoodnotesIPad] = [
        pencilP95,
        pencilP99,
        huge500ScrollJank,
        huge500Reopen,
        searchTenThousand,
        export500PDF,
        lassoTenThousand,
    ]

    static func escenario(idVersionado: String) -> EscenarioComparativaGoodnotesIPad? {
        todos.first { $0.idVersionado == idVersionado }
    }

    static func validarCatalogo() -> Bool {
        guard Set(todos.map(\.idVersionado)).count == todos.count else { return false }
        return todos.allSatisfy { escenario in
            escenario.version > 0
                && escenario.minimoMuestras > 0
                && escenario.warmup >= 0
                && !escenario.nombre.isEmpty
                && !escenario.metrica.isEmpty
                && !escenario.unidad.isEmpty
                && !escenario.descripcionCorpus.isEmpty
                && !escenario.pasos.isEmpty
        }
    }
}
