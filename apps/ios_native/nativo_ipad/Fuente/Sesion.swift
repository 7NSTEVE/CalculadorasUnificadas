import Foundation
import QuartzCore

enum FormaSeleccionLienzo: Int, CaseIterable, Codable {
    case lazoLibre
    case rectangulo
    case elipse
    case pincel

    var nombre: String {
        switch self {
        case .lazoLibre: return "Lazo libre"
        case .rectangulo: return "Rectángulo"
        case .elipse: return "Elipse"
        case .pincel: return "Pincel de selección"
        }
    }
}

enum FiltroSeleccionLienzo: Int32, CaseIterable, Codable {
    case todo = 0
    case tinta = 1
    case texto = 2
    case imagen = 3
    case pdf = 4

    var nombre: String {
        switch self {
        case .todo: return "Todo"
        case .tinta: return "Solo tinta"
        case .texto: return "Solo texto"
        case .imagen: return "Solo imágenes"
        case .pdf: return "Solo PDF"
        }
    }
}

enum CombinacionSeleccionLienzo: Int, CaseIterable, Codable {
    case reemplazar
    case agregar
    case restar

    var nombre: String {
        switch self {
        case .reemplazar: return "Reemplazar"
        case .agregar: return "Agregar"
        case .restar: return "Restar"
        }
    }
}

struct AjustesSeleccionLienzo: Codable, Equatable {
    var forma: FormaSeleccionLienzo = .lazoLibre
    var filtro: FiltroSeleccionLienzo = .todo
    var combinacion: CombinacionSeleccionLienzo = .reemplazar
    var requerirCompleto = true
    var colorARGB: UInt32 = 0
    var radioPincel: CGFloat = 24
    var ajustarCuadricula = false
    var pasoCuadricula: CGFloat = 16
}

struct SeleccionLienzo: Equatable, Codable {
    var trazos: [Int] = []
    var textos: [Int] = []
    var medios: [Int] = []

    var total: Int { trazos.count + textos.count + medios.count }
    var vacia: Bool { total == 0 }

    func combinada(con otra: SeleccionLienzo,
                   modo: CombinacionSeleccionLienzo) -> SeleccionLienzo {
        func combinar(_ a: [Int], _ b: [Int]) -> [Int] {
            let primero = Set(a), segundo = Set(b)
            switch modo {
            case .reemplazar: return segundo.sorted()
            case .agregar: return primero.union(segundo).sorted()
            case .restar: return primero.subtracting(segundo).sorted()
            }
        }
        return SeleccionLienzo(trazos: combinar(trazos, otra.trazos),
                               textos: combinar(textos, otra.textos),
                               medios: combinar(medios, otra.medios))
    }
}

/// Envoltorio Swift de la sesión de tinta del núcleo C.
///
/// Su única responsabilidad es que el puntero se libere: todo lo demás es una
/// llamada directa a C, sin capa intermedia. Cuanto más delgado sea este
/// archivo, menos código hay que solo se pueda verificar gastando runner macOS.
final class Sesion {

    private func marcarTiempoCronologia() {
        tinta_cronologia_fijar_tiempo(puntero, CACurrentMediaTime())
    }

    /// Identidad durable que pertenece al documento, no al proceso.
    let idPersistente: DocumentoPersistenteID

    /// Clave transitoria de compatibilidad para sidecars UInt64 existentes.
    /// No es el handle del núcleo y no cambia al reabrir el documento.
    var documentoPersistenteId: UInt64 {
        idPersistente.claveCompatibilidadUInt64
    }

    /// `TintaDocumentoId` del núcleo: handle efímero, solo para llamadas C.
    let manejadorNucleo: TintaDocumentoId

    /// `TintaSesion *` del núcleo. Swift lo ve como puntero opaco.
    let puntero: OpaquePointer

    init?(idPersistente: DocumentoPersistenteID = DocumentoPersistenteID()) {
        // En la Fase 2, el núcleo gestiona los documentos globalmente.
        let id = tinta_documento_abrir()
        guard id != 0 else {
            print("Error: El nucleo de tinta no pudo crear el documento")
            return nil
        }
        guard let p = tinta_documento_sesion(id) else {
            print("Error: El nucleo de tinta devolvio una sesion nula para el documento")
            tinta_documento_cerrar(id)
            return nil
        }
        guard idPersistente.description.withCString({
            tinta_documento_asignar_identidad(p, $0)
        }) else {
            print("Error: El núcleo rechazó la identidad durable del documento")
            tinta_documento_cerrar(id)
            return nil
        }
        self.idPersistente = idPersistente
        self.manejadorNucleo = id
        self.puntero = p
    }
    
    init?(manejadorNucleo: TintaDocumentoId,
          idPersistente: DocumentoPersistenteID) {
        self.idPersistente = idPersistente
        self.manejadorNucleo = manejadorNucleo
        guard let p = tinta_documento_sesion(manejadorNucleo) else {
            print("Error: El nucleo de tinta devolvio una sesion nula para el handle \(manejadorNucleo)")
            return nil
        }
        guard idPersistente.description.withCString({
            tinta_documento_asignar_identidad(p, $0)
        }) else {
            print("Error: El núcleo rechazó la identidad durable del documento")
            tinta_documento_cerrar(manejadorNucleo)
            return nil
        }
        self.puntero = p
    }

    deinit {
        tinta_documento_cerrar(manejadorNucleo)
    }

    // MARK: - Trazo

    func comenzarTrazo(ancho: Float, color: UInt32, resaltador: Bool) {
        tinta_comenzar_trazo(puntero, ancho, color, resaltador)
    }

    func anadirMuestra(x: Float, y: Float, presion: Float,
                       altitud: Float, azimut: Float, tiempo: Double) {
        tinta_anadir_muestra(puntero, x, y, presion, altitud, azimut, tiempo)
    }

    /// Añade una muestra que UIKit todavía puede corregir y devuelve su
    /// identidad efímera dentro del núcleo.
    func anadirMuestraActualizable(x: Float, y: Float, presion: Float,
                                   altitud: Float, azimut: Float,
                                   tiempo: Double) -> UInt64? {
        let id = tinta_anadir_muestra_actualizable(
            puntero, x, y, presion, altitud, azimut, tiempo)
        return id == 0 ? nil : id
    }

    @discardableResult
    func actualizarMuestra(_ id: UInt64, propiedades: UInt32,
                           x: Float, y: Float, presion: Float,
                           altitud: Float, azimut: Float) -> Bool {
        tinta_actualizar_muestra(
            puntero, id, propiedades, x, y, presion, altitud, azimut)
    }

    func finalizarActualizacionesMuestra(_ id: UInt64) {
        _ = tinta_finalizar_actualizaciones_muestra(puntero, id)
    }

    func limpiarProvisionales() {
        tinta_limpiar_provisionales(puntero)
    }

    func anadirProvisional(x: Float, y: Float, presion: Float,
                           altitud: Float, azimut: Float, tiempo: Double) {
        tinta_anadir_provisional(puntero, x, y, presion, altitud, azimut, tiempo)
    }

    func terminarTrazo(id: String? = nil) -> Int? {
        let indice: Int64
        if let id {
            indice = id.withCString {
                tinta_terminar_trazo_con_id(puntero, $0)
            }
        } else {
            indice = tinta_terminar_trazo(puntero)
        }
        return indice < 0 ? nil : Int(indice)
    }
    func descartarTrazo() { tinta_descartar_trazo(puntero) }

    func configurarPluma(_ ajustes: TintaAjustesPluma) {
        tinta_pluma_configurar(puntero, ajustes)
    }

    func refinarTrazo(_ indice: Int, nivel: Int) -> Bool {
        tinta_trazo_refinar(puntero, indice, Int32(nivel))
    }

    func refinarTodos(nivel: Int) -> Bool {
        tinta_refinar_todos(puntero, Int32(nivel))
    }

    // MARK: - Predicción

    var prediccionActiva: Bool = true {
        didSet { tinta_set_prediccion(puntero, prediccionActiva) }
    }

    var horizonteMs: Float = 12 {
        didSet { tinta_set_horizonte_ms(puntero, horizonteMs) }
    }

    var muestrasPredichas: Int { tinta_muestras_predichas(puntero) }

    // MARK: - Estado

    var cantidadTrazos: Int { tinta_cantidad_trazos(puntero) }
    var hayTrazoEnCurso: Bool { tinta_hay_trazo_en_curso(puntero) }
    var frecuenciaHz: Double { tinta_frecuencia_hz(puntero) }
    var colorTrazoEnCurso: UInt32 { tinta_color_en_curso(puntero) }

    func idTrazo(_ indice: Int) -> String? {
        guard let texto = tinta_trazo_id(puntero, indice) else { return nil }
        let valor = String(cString: texto)
        return valor.isEmpty ? nil : valor
    }

    func indiceTrazo(id: String) -> Int? {
        guard !id.isEmpty else { return nil }
        return (0..<cantidadTrazos).first { idTrazo($0) == id }
    }

    @discardableResult
    func asegurarIdTrazo(_ indice: Int, sugerido: String = UUID().uuidString) -> String? {
        if let existente = idTrazo(indice) { return existente }
        let asignado = sugerido.withCString { tinta_trazo_asignar_id(puntero, indice, $0) }
        return asignado ? sugerido : nil
    }

    func serializarTrazo(_ indice: Int) -> Data? {
        var cantidad = 0
        guard let bytes = tinta_trazo_serializar(puntero, indice, &cantidad),
              cantidad > 0 else { return nil }
        return Data(bytes: bytes, count: cantidad)
    }

    func puntosTrazo(_ indice: Int) -> [CGPoint] {
        var cantidad = 0
        guard let valores = tinta_trazo_puntos(puntero, indice, &cantidad),
              cantidad > 0 else { return [] }
        return (0..<cantidad).map {
            CGPoint(x: CGFloat(valores[$0 * 2]), y: CGFloat(valores[$0 * 2 + 1]))
        }
    }

    @discardableResult
    func importarTrazoColaborativo(_ datos: Data) -> Bool {
        marcarTiempoCronologia()
        return datos.withUnsafeBytes { crudo in
            guard let base = crudo.bindMemory(to: UInt8.self).baseAddress else {
                return false
            }
            return tinta_trazo_importar(puntero, base, crudo.count)
        }
    }

    @discardableResult
    func importarTrazoComoCopia(_ datos: Data,
                                id: String = UUID().uuidString) -> Bool {
        marcarTiempoCronologia()
        return datos.withUnsafeBytes { crudo in
            guard let base = crudo.bindMemory(to: UInt8.self).baseAddress else {
                return false
            }
            return id.withCString {
                tinta_trazo_importar_copia(puntero, base, crudo.count, $0)
            }
        }
    }

    @discardableResult
    func eliminarTrazoColaborativo(id: String) -> Bool {
        marcarTiempoCronologia()
        return id.withCString { tinta_trazo_eliminar_id(puntero, $0) }
    }

    var rangoTemporal: ClosedRange<Double>? {
        var inicio = 0.0
        var fin = 0.0
        guard tinta_rango_temporal(puntero, &inicio, &fin) else { return nil }
        return inicio...fin
    }

    // MARK: - Geometría para la GPU

    /// Triángulos del trazo que se está escribiendo ahora mismo.
    ///
    /// El puntero pertenece al núcleo y vale **hasta la siguiente llamada** a
    /// cualquier función de la API: se copia al buffer de Metal y se olvida.
    func verticesTrazoEnCurso() -> (datos: UnsafePointer<Float>, cantidad: Int)? {
        var cantidad = 0
        guard let datos = tinta_vertices_en_curso(puntero, &cantidad), cantidad > 0 else {
            return nil
        }
        return (datos, cantidad)
    }

    func verticesDelTrazo(_ indice: Int)
        -> (datos: UnsafePointer<Float>, cantidad: Int, color: UInt32)? {
        var cantidad = 0
        var color: UInt32 = 0
        guard let datos = tinta_vertices_trazo(puntero, indice, &cantidad, &color),
              cantidad > 0 else { return nil }
        return (datos, cantidad, color)
    }

    func verticesDelTrazo(_ indice: Int, hasta tiempo: Double)
        -> (datos: UnsafePointer<Float>, cantidad: Int, color: UInt32)? {
        var cantidad = 0
        var color: UInt32 = 0
        guard let datos = tinta_vertices_trazo_hasta(
            puntero, indice, tiempo, &cantidad, &color), cantidad > 0 else {
            return nil
        }
        return (datos, cantidad, color)
    }

    // MARK: - Edición

    func deshacer() { marcarTiempoCronologia(); tinta_deshacer(puntero) }
    func rehacer() { marcarTiempoCronologia(); tinta_rehacer(puntero) }
    func limpiar() { marcarTiempoCronologia(); tinta_limpiar(puntero) }
    var puedeDeshacer: Bool { tinta_puede_deshacer(puntero) }

    @discardableResult
    func borrar(x: Float, y: Float, radio: Float, parcial: Bool) -> Bool {
        marcarTiempoCronologia()
        return tinta_borrar(puntero, x, y, radio, parcial)
    }

    @discardableResult
    func borrar(x: Float, y: Float, radio: Float, parcial: Bool,
                filtro: FiltroBorradorAvanzadoIPad,
                colorARGB: UInt32) -> Bool {
        marcarTiempoCronologia()
        let filtroNucleo: Int32 = filtro == .soloCapaActual
            ? FiltroBorradorAvanzadoIPad.todos.rawValue : filtro.rawValue
        return tinta_borrar_filtrado(
            puntero, x, y, radio, parcial, filtroNucleo, colorARGB)
    }

    func seleccionarTrazos(lazo: [CGPoint], completo: Bool = true) -> [Int] {
        guard lazo.count >= 3 else { return [] }
        var puntos = [Float]()
        puntos.reserveCapacity(lazo.count * 2)
        for punto in lazo {
            puntos.append(Float(punto.x))
            puntos.append(Float(punto.y))
        }
        var cantidad = 0
        return puntos.withUnsafeBufferPointer { buffer in
            guard let base = buffer.baseAddress,
                  let indices = tinta_lazo_seleccionar_trazos(
                    puntero, base, lazo.count, completo, &cantidad),
                  cantidad > 0 else { return [] }
            return (0..<cantidad).map { Int(indices[$0]) }
        }
    }

    private func seleccion(_ info: TintaSeleccionInfo) -> SeleccionLienzo {
        func copiar(_ puntero: UnsafePointer<Int>?, _ cantidad: Int) -> [Int] {
            guard let puntero, cantidad > 0 else { return [] }
            return Array(UnsafeBufferPointer(start: puntero, count: cantidad))
        }
        return SeleccionLienzo(
            trazos: copiar(info.trazos, info.cantidad_trazos),
            textos: copiar(info.textos, info.cantidad_textos),
            medios: copiar(info.medios, info.cantidad_medios))
    }

    private func puntosFlotantes(_ puntos: [CGPoint]) -> [Float] {
        puntos.flatMap { [Float($0.x), Float($0.y)] }
    }

    func seleccionarPoligono(_ puntos: [CGPoint],
                             ajustes: AjustesSeleccionLienzo) -> SeleccionLienzo {
        guard puntos.count >= 3 else { return SeleccionLienzo() }
        let valores = puntosFlotantes(puntos)
        return valores.withUnsafeBufferPointer { buffer in
            seleccion(tinta_seleccion_poligono(
                puntero, buffer.baseAddress, puntos.count,
                Int32(ajustes.filtro.rawValue), ajustes.colorARGB,
                ajustes.requerirCompleto))
        }
    }

    func seleccionarRectangulo(_ rectangulo: CGRect,
                               ajustes: AjustesSeleccionLienzo) -> SeleccionLienzo {
        seleccion(tinta_seleccion_rectangulo(
            puntero, Float(rectangulo.minX), Float(rectangulo.minY),
            Float(rectangulo.width), Float(rectangulo.height),
            Int32(ajustes.filtro.rawValue), ajustes.colorARGB,
            ajustes.requerirCompleto))
    }

    func seleccionarElipse(_ rectangulo: CGRect,
                           ajustes: AjustesSeleccionLienzo) -> SeleccionLienzo {
        seleccion(tinta_seleccion_elipse(
            puntero, Float(rectangulo.midX), Float(rectangulo.midY),
            Float(rectangulo.width / 2), Float(rectangulo.height / 2),
            Int32(ajustes.filtro.rawValue), ajustes.colorARGB,
            ajustes.requerirCompleto))
    }

    func seleccionarPincel(_ puntos: [CGPoint],
                           ajustes: AjustesSeleccionLienzo) -> SeleccionLienzo {
        guard !puntos.isEmpty else { return SeleccionLienzo() }
        let valores = puntosFlotantes(puntos)
        return valores.withUnsafeBufferPointer { buffer in
            seleccion(tinta_seleccion_pincel(
                puntero, buffer.baseAddress, puntos.count,
                Float(max(ajustes.radioPincel, 1)),
                Int32(ajustes.filtro.rawValue), ajustes.colorARGB,
                ajustes.requerirCompleto))
        }
    }

    func seleccionarTodo(filtro: FiltroSeleccionLienzo = .todo,
                         colorARGB: UInt32 = 0) -> SeleccionLienzo {
        seleccion(tinta_seleccion_todo(puntero, filtro.rawValue, colorARGB))
    }

    @discardableResult
    func transformarSeleccion(_ seleccionActual: SeleccionLienzo,
                              desplazamiento: CGSize = .zero,
                              escalaX: CGFloat = 1, escalaY: CGFloat = 1,
                              rotacion: CGFloat = 0, pivote: CGPoint = .zero,
                              reflejarHorizontal: Bool = false,
                              reflejarVertical: Bool = false) -> Bool {
        marcarTiempoCronologia()
        let trazos = seleccionActual.trazos
        let textos = seleccionActual.textos
        let medios = seleccionActual.medios
        return trazos.withUnsafeBufferPointer { bt in
            textos.withUnsafeBufferPointer { bx in
                medios.withUnsafeBufferPointer { bm in
                    tinta_seleccion_transformar(
                        puntero, bt.baseAddress, bt.count,
                        bx.baseAddress, bx.count, bm.baseAddress, bm.count,
                        Float(desplazamiento.width), Float(desplazamiento.height),
                        Float(escalaX), Float(escalaY), Float(rotacion),
                        Float(pivote.x), Float(pivote.y), reflejarHorizontal,
                        reflejarVertical)
                }
            }
        }
    }

    @discardableResult
    func eliminarSeleccion(_ seleccionActual: SeleccionLienzo) -> Bool {
        marcarTiempoCronologia()
        let trazos = seleccionActual.trazos
        let textos = seleccionActual.textos
        let medios = seleccionActual.medios
        return trazos.withUnsafeBufferPointer { bt in
            textos.withUnsafeBufferPointer { bx in
                medios.withUnsafeBufferPointer { bm in
                    tinta_seleccion_eliminar(
                        puntero, bt.baseAddress, bt.count, bx.baseAddress,
                        bx.count, bm.baseAddress, bm.count)
                }
            }
        }
    }

    func duplicarSeleccion(_ seleccionActual: SeleccionLienzo,
                           desplazamiento: CGSize = CGSize(width: 28, height: 28))
        -> SeleccionLienzo {
        marcarTiempoCronologia()
        let trazos = seleccionActual.trazos
        let textos = seleccionActual.textos
        let medios = seleccionActual.medios
        return trazos.withUnsafeBufferPointer { bt in
            textos.withUnsafeBufferPointer { bx in
                medios.withUnsafeBufferPointer { bm in
                    seleccion(tinta_seleccion_duplicar(
                        puntero, bt.baseAddress, bt.count, bx.baseAddress,
                        bx.count, bm.baseAddress, bm.count,
                        Float(desplazamiento.width), Float(desplazamiento.height)))
                }
            }
        }
    }

    func ordenarSeleccion(_ seleccionActual: SeleccionLienzo,
                          alFrente: Bool) -> SeleccionLienzo {
        marcarTiempoCronologia()
        let trazos = seleccionActual.trazos
        let textos = seleccionActual.textos
        let medios = seleccionActual.medios
        return trazos.withUnsafeBufferPointer { bt in
            textos.withUnsafeBufferPointer { bx in
                medios.withUnsafeBufferPointer { bm in
                    seleccion(tinta_seleccion_ordenar(
                        puntero, bt.baseAddress, bt.count, bx.baseAddress,
                        bx.count, bm.baseAddress, bm.count, alFrente))
                }
            }
        }
    }

    @discardableResult
    func transformarTrazos(_ indices: [Int], desplazamiento: CGSize,
                           escala: CGFloat = 1, rotacion: CGFloat = 0,
                           pivote: CGPoint = .zero,
                           reflejarHorizontal: Bool = false,
                           reflejarVertical: Bool = false) -> Bool {
        marcarTiempoCronologia()
        let valores = indices.filter { $0 >= 0 }
        return valores.withUnsafeBufferPointer { buffer in
            guard let base = buffer.baseAddress, !buffer.isEmpty else { return false }
            return tinta_trazos_transformar(
                puntero, base, buffer.count,
                Float(desplazamiento.width), Float(desplazamiento.height),
                Float(escala), Float(rotacion), Float(pivote.x), Float(pivote.y),
                reflejarHorizontal, reflejarVertical)
        }
    }

    @discardableResult
    func eliminarTrazos(_ indices: [Int]) -> Bool {
        marcarTiempoCronologia()
        let valores = indices.filter { $0 >= 0 }
        return valores.withUnsafeBufferPointer { buffer in
            guard let base = buffer.baseAddress, !buffer.isEmpty else { return false }
            return tinta_trazos_eliminar(puntero, base, buffer.count)
        }
    }

    @discardableResult
    func recolorearTrazos(_ indices: [Int], color: UInt32) -> Bool {
        marcarTiempoCronologia()
        let valores = indices.filter { $0 >= 0 }
        return valores.withUnsafeBufferPointer { buffer in
            guard let base = buffer.baseAddress, !buffer.isEmpty else { return false }
            return tinta_trazos_recolorear(puntero, base, buffer.count, color)
        }
    }

    /// Reconoce el último trazo y lo endereza. Devuelve la forma reconocida.
    @discardableResult
    func enderezarUltimoTrazo() -> FormaReconocida {
        marcarTiempoCronologia()
        return FormaReconocida(
            rawValue: tinta_enderezar_ultimo_trazo(puntero)) ?? .ninguna
    }

    // MARK: - Papel

    func configurarPapel(tipo: TipoPapel, pauta: Pauta,
                         separacion: Float, margen: Float) {
        marcarTiempoCronologia()
        tinta_set_papel(puntero, tipo.rawValue, pauta.rawValue, separacion, margen)
    }

    func configurarPapelAvanzado(_ ajustes: AjustesPapel) {
        marcarTiempoCronologia()
        tinta_set_papel_avanzado(
            puntero, ajustes.pauta.rawValue, ajustes.separacion,
            ajustes.opacidad, ajustes.grosor, ajustes.colorARGB,
            ajustes.margenIzquierdo, ajustes.margenDerecho,
            ajustes.margenSuperior, ajustes.margenInferior,
            ajustes.mostrarEjes, ajustes.ejeX, ajustes.ejeY,
            ajustes.incluirAlExportar)
    }

    var colorPapel: UInt32 { tinta_color_papel(puntero) }
    var colorPauta: UInt32 { tinta_color_pauta(puntero) }
    /// En papel oscuro devuelve una tinta clara: escribir negro sobre negro es
    /// el error fácil.
    var colorTintaSugerido: UInt32 { tinta_color_tinta_sugerido(puntero) }

    /// Segmentos de la pauta visibles, como cuartetos (x0, y0, x1, y1).
    func pautaVisible(x: Float, y: Float, ancho: Float, alto: Float)
        -> UnsafeBufferPointer<Float> {
        var segmentos = 0
        guard let datos = tinta_pauta_visible(puntero, x, y, ancho, alto, &segmentos),
              segmentos > 0 else {
            return UnsafeBufferPointer(start: nil, count: 0)
        }
        return UnsafeBufferPointer(start: datos, count: segmentos * 4)
    }

    /// Copia los segmentos porque el búfer C deja de ser válido al hacer la
    /// siguiente llamada al núcleo.
    func pautaVisibleAvanzada(x: Float, y: Float, ancho: Float, alto: Float)
        -> [SegmentoPauta] {
        var cantidad = 0
        guard let datos = tinta_pauta_visible_avanzada(
            puntero, x, y, ancho, alto, &cantidad), cantidad > 0 else { return [] }
        return (0..<cantidad).map { indice in
            let base = indice * 5
            return SegmentoPauta(
                desde: CGPoint(x: CGFloat(datos[base]), y: CGFloat(datos[base + 1])),
                hasta: CGPoint(x: CGFloat(datos[base + 2]), y: CGFloat(datos[base + 3])),
                destacado: datos[base + 4] > 0.5)
        }
    }

    // MARK: - Lienzo

    func configurarLimites(anchoInfinito: Bool, altoInfinito: Bool,
                           anchoMin: Float, anchoMax: Float,
                           altoMin: Float, altoMax: Float) {
        marcarTiempoCronologia()
        tinta_set_limites(puntero, anchoInfinito, altoInfinito,
                          anchoMin, anchoMax, altoMin, altoMax)
    }

    var anchoPagina: Float { tinta_ancho_pagina(puntero) }
    var altoPagina: Float { tinta_alto_pagina(puntero) }
    var anchoInfinito: Bool { tinta_pagina_ancho_infinito(puntero) }
    var altoInfinito: Bool { tinta_pagina_alto_infinito(puntero) }

    func aplicarModoPagina(_ modo: ModoPaginaLienzo) {
        marcarTiempoCronologia()
        tinta_pagina_aplicar_modo(puntero, modo.rawValue)
    }

    @discardableResult
    func cambiarTamano(ancho: Float, alto: Float, unidad: UnidadLienzo,
                       ppp: Float = 150, reacomodo: ReacomodoLienzo = .conservar) -> Bool {
        marcarTiempoCronologia()
        return tinta_pagina_cambiar_tamano(
            puntero, ancho, alto, unidad.rawValue, ppp, reacomodo.rawValue)
    }

    @discardableResult
    func ajustarAlContenido(margen: Float = 24) -> Bool {
        marcarTiempoCronologia()
        return tinta_pagina_ajustar_al_contenido(puntero, margen)
    }

    @discardableResult
    func insertarEspacio(eje: EjeLienzo, desde: Float, cantidad: Float) -> Bool {
        marcarTiempoCronologia()
        return tinta_pagina_insertar_espacio(
            puntero, eje.rawValue, desde, cantidad)
    }

    func planReflujo(region: CGRect, factor: Float, modo: Int) -> [TintaTransformacionTrazo] {
        let total = tinta_reflujo_planificar(
            puntero, Float(region.minX), Float(region.minY), Float(region.width),
            Float(region.height), factor, Int32(modo), nil, 0)
        guard total > 0 else { return [] }
        var salida = [TintaTransformacionTrazo](
            repeating: TintaTransformacionTrazo(), count: total)
        let capacidad = salida.count
        let escritos = tinta_reflujo_planificar(
            puntero, Float(region.minX), Float(region.minY), Float(region.width),
            Float(region.height), factor, Int32(modo), &salida, capacidad)
        return Array(salida.prefix(min(escritos, capacidad)))
    }

    func aplicarReflujo(_ plan: [TintaTransformacionTrazo]) -> Bool {
        marcarTiempoCronologia()
        let copia = plan
        return copia.withUnsafeBufferPointer {
            tinta_reflujo_aplicar(puntero, $0.baseAddress, $0.count)
        }
    }

    func marcoTrazo(_ indice: Int) -> CGRect? {
        guard let v = verticesDelTrazo(indice), v.cantidad > 0 else { return nil }
        var minX = CGFloat.greatestFiniteMagnitude, minY = minX
        var maxX = -minX, maxY = -minY
        for i in 0..<v.cantidad {
            let x = CGFloat(v.datos[i * 2]), y = CGFloat(v.datos[i * 2 + 1])
            minX = min(minX, x); minY = min(minY, y)
            maxX = max(maxX, x); maxY = max(maxY, y)
        }
        return CGRect(x: minX, y: minY, width: max(maxX - minX, 1),
                      height: max(maxY - minY, 1))
    }

    @discardableResult
    func moverBorde(_ borde: BordeLienzo, delta: Float,
                    desplazarContenido: Bool = true) -> Bool {
        marcarTiempoCronologia()
        return tinta_pagina_mover_borde(
            puntero, borde.rawValue, delta, desplazarContenido)
    }

    func cortesImpresion(altoHoja: Float, margenSeguro: Float) -> [Float] {
        var cantidad = 0
        guard let datos = tinta_pagina_cortes_impresion(
            puntero, altoHoja, margenSeguro, &cantidad), cantidad > 0 else { return [] }
        return Array(UnsafeBufferPointer(start: datos, count: cantidad))
    }

    // MARK: - Elementos colocables

    @discardableResult
    func anadirTexto(_ texto: String, marco: CGRect, dictado: Bool = false,
                     id: String = UUID().uuidString) -> String? {
        marcarTiempoCronologia()
        let exito = id.withCString { idC in
            texto.withCString { textoC in
                tinta_texto_anadir(
                    puntero, idC, textoC, Float(marco.minX), Float(marco.minY),
                    Float(marco.width), Float(marco.height), dictado)
            }
        }
        return exito ? id : nil
    }

    var textos: [ElementoTexto] {
        (0..<tinta_texto_cantidad(puntero)).compactMap { indice in
            let info = tinta_texto_info(puntero, indice)
            guard let id = info.id, let texto = info.texto else { return nil }
            return ElementoTexto(
                id: String(cString: id), texto: String(cString: texto),
                marco: CGRect(x: CGFloat(info.x), y: CGFloat(info.y),
                              width: CGFloat(info.ancho), height: CGFloat(info.alto)),
                rotacion: CGFloat(info.rotacion),
                altoAutomatico: info.alto_automatico,
                desdeDictado: info.desde_dictado)
        }
    }

    /// Analiza el documento buscando textos que parezcan títulos (por su altura)
    /// y genera un índice de temas calculando la "página" basada en la altura proporcionada.
    func generarIndiceAutomatico(altoPagina: CGFloat) -> [TemaIndice] {
        var temas: [TemaIndice] = []
        let todosLosTextos = self.textos
        
        // Criterio simple: si el texto tiene una altura mayor a 35, es un título.
        // O si es el único texto en un rango amplio.
        for texto in todosLosTextos {
            if texto.marco.height > 35 {
                let pagina = Int(texto.marco.minY / altoPagina) + 1
                let tituloLimpio = texto.texto.trimmingCharacters(in: .whitespacesAndNewlines)
                    .components(separatedBy: .newlines).first ?? "Sin título"
                
                temas.append(TemaIndice(
                    titulo: tituloLimpio,
                    pagina: pagina,
                    yOffset: texto.marco.minY
                ))
            }
        }
        
        // Ordenar por aparición en el documento
        return temas.sorted { $0.yOffset < $1.yOffset }
    }

    @discardableResult
    func actualizarTexto(_ elemento: ElementoTexto) -> Bool {
        marcarTiempoCronologia()
        return elemento.id.withCString { idC in
            elemento.texto.withCString { textoC in
                tinta_texto_actualizar(
                    puntero, idC, textoC, Float(elemento.marco.minX),
                    Float(elemento.marco.minY), Float(elemento.marco.width),
                    Float(elemento.marco.height), Float(elemento.rotacion))
            }
        }
    }

    @discardableResult
    func eliminarTexto(id: String) -> Bool {
        marcarTiempoCronologia()
        return id.withCString { tinta_texto_eliminar(puntero, $0) }
    }

    @discardableResult
    func anadirMedio(hash: String, nombre: String, tipo: TipoMedio,
                     marco: CGRect, id: String = UUID().uuidString) -> String? {
        marcarTiempoCronologia()
        let exito = id.withCString { idC in
            hash.withCString { hashC in
                nombre.withCString { nombreC in
                    tinta_medio_anadir(
                        puntero, idC, hashC, nombreC, tipo.rawValue,
                        Float(marco.minX), Float(marco.minY),
                        Float(marco.width), Float(marco.height))
                }
            }
        }
        return exito ? id : nil
    }

    var medios: [ElementoMedio] {
        (0..<tinta_medio_cantidad(puntero)).compactMap { indice in
            let info = tinta_medio_info(puntero, indice)
            guard let id = info.id, let hash = info.hash,
                  let nombre = info.nombre_original,
                  let tipo = TipoMedio(rawValue: info.tipo) else { return nil }
            return ElementoMedio(
                id: String(cString: id), hash: String(cString: hash),
                nombre: String(cString: nombre), tipo: tipo,
                marco: CGRect(x: CGFloat(info.x), y: CGFloat(info.y),
                              width: CGFloat(info.ancho), height: CGFloat(info.alto)),
                rotacion: CGFloat(info.rotacion), opacidad: CGFloat(info.opacidad),
                recorte: Medios.Recorte(x: info.recorte_x, y: info.recorte_y,
                                       ancho: info.recorte_ancho, alto: info.recorte_alto),
                paginaFuente: Int(info.pagina_fuente),
                segundoInicio: info.segundo_inicio)
        }
    }

    @discardableResult
    func actualizarMedio(_ medio: ElementoMedio) -> Bool {
        marcarTiempoCronologia()
        return medio.id.withCString {
            tinta_medio_actualizar_marco(
                puntero, $0, Float(medio.marco.minX), Float(medio.marco.minY),
                Float(medio.marco.width), Float(medio.marco.height),
                Float(medio.rotacion), Float(medio.opacidad))
        }
    }

    @discardableResult
    func configurarFuenteMedio(id: String, pagina: Int = 0,
                               segundo: Double = 0) -> Bool {
        marcarTiempoCronologia()
        return id.withCString {
            tinta_medio_set_fuente(puntero, $0, Int32(pagina), segundo)
        }
    }

    @discardableResult
    func recortarMedio(id: String, recorteRelativo: CGRect) -> Bool {
        marcarTiempoCronologia()
        return id.withCString {
            tinta_medio_recortar(
                puntero, $0, Float(recorteRelativo.minX), Float(recorteRelativo.minY),
                Float(recorteRelativo.width), Float(recorteRelativo.height))
        }
    }

    @discardableResult
    func quitarRecorteMedio(id: String) -> Bool {
        marcarTiempoCronologia()
        return id.withCString { tinta_medio_quitar_recorte(puntero, $0) }
    }

    @discardableResult
    func eliminarMedio(id: String) -> Bool {
        marcarTiempoCronologia()
        return id.withCString { tinta_medio_eliminar(puntero, $0) }
    }

    func reconocerUltimosTrazosComoCircuito(_ cantidad: Int)
        -> ReconocimientoCircuito? {
        var salida = TintaReconocimientoCircuito()
        guard tinta_circuito_reconocer_ultimos(puntero, cantidad, &salida) else {
            return nil
        }
        return ReconocimientoCircuito(
            tipo: TipoComponenteElectrico(rawValue: salida.tipo) ?? .cable,
            confianza: salida.confianza,
            terminalA: CGPoint(x: CGFloat(salida.terminal_ax),
                               y: CGFloat(salida.terminal_ay)),
            terminalB: CGPoint(x: CGFloat(salida.terminal_bx),
                               y: CGFloat(salida.terminal_by)))
    }

    func reconocerSeleccionComoCircuito(_ indices: [Int])
        -> ReconocimientoCircuito? {
        let valores = indices.filter { $0 >= 0 }
        var salida = TintaReconocimientoCircuito()
        let reconocido = valores.withUnsafeBufferPointer { buffer in
            guard let base = buffer.baseAddress, !buffer.isEmpty else { return false }
            return tinta_circuito_reconocer_seleccion(
                puntero, base, buffer.count, &salida)
        }
        guard reconocido else { return nil }
        return ReconocimientoCircuito(
            tipo: TipoComponenteElectrico(rawValue: salida.tipo) ?? .cable,
            confianza: salida.confianza,
            terminalA: CGPoint(x: CGFloat(salida.terminal_ax),
                               y: CGFloat(salida.terminal_ay)),
            terminalB: CGPoint(x: CGFloat(salida.terminal_bx),
                               y: CGFloat(salida.terminal_by)))
    }

    @discardableResult
    func crecerSiHaceFalta(margen: Float) -> Bool {
        marcarTiempoCronologia()
        return tinta_crecer_si_hace_falta(puntero, margen)
    }

    // MARK: - Guardar y cargar

    @discardableResult
    func comenzarCronologiaNotaViva(en tiempoMonotono: Double) -> Bool {
        tinta_cronologia_comenzar(puntero, tiempoMonotono)
    }

    func cancelarCronologiaNotaViva() {
        tinta_cronologia_cancelar(puntero)
    }

    var cantidadEventosCronologiaNotaViva: Int {
        tinta_cronologia_cantidad_eventos(puntero)
    }

    func finalizarCronologiaNotaViva() -> Data? {
        var cantidad = 0
        guard let datos = tinta_cronologia_finalizar(puntero, &cantidad),
              cantidad > 0 else { return nil }
        return Data(bytes: datos, count: cantidad)
    }

    @discardableResult
    func cargarCronologiaNotaViva(_ datos: Data) -> Bool {
        datos.withUnsafeBytes { crudo in
            guard let base = crudo.bindMemory(to: UInt8.self).baseAddress else {
                return false
            }
            return tinta_cronologia_cargar(puntero, base, crudo.count)
        }
    }

    /// `true` significa que cambió la composición (altas, ediciones o bajas).
    /// Un `false` válido todavía requiere avanzar el corte dentro del trazo.
    @discardableResult
    func mostrarCronologiaNotaViva(hasta tiempoMonotono: Double) -> Bool {
        tinta_cronologia_mostrar_hasta(puntero, tiempoMonotono)
    }

    func guardar() -> Data? {
        var cantidad = 0
        guard let datos = tinta_guardar(puntero, &cantidad), cantidad > 0 else {
            return nil
        }
        // Se copia: el puntero del núcleo vale solo hasta la siguiente llamada.
        return Data(bytes: datos, count: cantidad)
    }

    @discardableResult
    func cargar(_ datos: Data) -> Bool {
        datos.withUnsafeBytes { crudo in
            guard let base = crudo.bindMemory(to: UInt8.self).baseAddress else {
                return false
            }
            return tinta_cargar(puntero, base, crudo.count)
        }
    }

    /// Ruta explícita para duplicar/importar. La apertura normal jamás adopta
    /// bytes de otra identidad, porque eso volvería a permitir cruces de notas.
    /// La copia toma también el agregado documental local cuando el blob trae
    /// una identidad durable reconocible; un archivo legado sin UUID continúa
    /// importándose como tinta, sin inventar sidecars que no podemos demostrar.
    @discardableResult
    func cargarComoCopia(_ datos: Data) -> Bool {
        let origen = IdentidadCopiaDocumentoIPad.origen(de: datos)
        let cargado = datos.withUnsafeBytes { crudo in
            guard let base = crudo.bindMemory(to: UInt8.self).baseAddress else {
                return false
            }
            return idPersistente.description.withCString { identidad in
                tinta_cargar_como_copia(
                    puntero, base, crudo.count, identidad)
            }
        }
        guard cargado else { return false }
        return IdentidadCopiaDocumentoIPad.copiarAgregadoSiExiste(
            origen: origen, destino: idPersistente)
    }

    var ultimoError: String { String(cString: tinta_ultimo_error()) }
}

// Los enteros del API en C, con nombre.
enum TipoPapel: Int32, CaseIterable {
    case blanco = 0, blancoSol = 1, crema = 2, gris = 3, oscuro = 4, negro = 5

    var nombre: String {
        switch self {
        case .blanco: return "Blanco"
        case .blancoSol: return "Blanco cálido"
        case .crema: return "Crema"
        case .gris: return "Gris"
        case .oscuro: return "Oscuro"
        case .negro: return "Negro"
        }
    }
}

enum Pauta: Int32, CaseIterable {
    case ninguna = 0
    case rayado = 1
    case cuadricula = 2
    case puntos = 3
    case pentagrama = 4
    case milimetrico = 5
    case isometrico = 6
    case cornell = 7
    case logaritmico = 8
    case semiLogaritmico = 9
    case polar = 10
    case ingenieria = 11
    case circuitos = 12
    case cartesiano = 13
    case smith = 14
    case hexagonal = 15
    case tabla = 16
    case personalizada = 17

    var nombre: String {
        switch self {
        case .ninguna: return "Sin pauta"
        case .rayado: return "Rayado"
        case .cuadricula: return "Cuadrícula"
        case .puntos: return "Puntos"
        case .pentagrama: return "Pentagrama"
        case .milimetrico: return "Milimétrico"
        case .isometrico: return "Isométrico"
        case .cornell: return "Cornell"
        case .logaritmico: return "Logarítmico"
        case .semiLogaritmico: return "Semilogarítmico"
        case .polar: return "Polar"
        case .ingenieria: return "Ingeniería"
        case .circuitos: return "Circuitos"
        case .cartesiano: return "Cartesiano"
        case .smith: return "Carta de Smith"
        case .hexagonal: return "Hexagonal"
        case .tabla: return "Tabla"
        case .personalizada: return "Personalizada"
        }
    }
}

struct AjustesPapel {
    var pauta: Pauta = .ninguna
    var separacion: Float = 32
    var opacidad: Float = 0.45
    var grosor: Float = 1
    var colorARGB: UInt32 = 0
    var margenIzquierdo: Float = 0
    var margenDerecho: Float = 0
    var margenSuperior: Float = 0
    var margenInferior: Float = 0
    var mostrarEjes = false
    var ejeX: Float = 620
    var ejeY: Float = 877
    var incluirAlExportar = true
}

struct SegmentoPauta {
    let desde: CGPoint
    let hasta: CGPoint
    let destacado: Bool
}

enum ModoPaginaLienzo: Int32, CaseIterable {
    case libro = 0, infinitoXY = 1, paginadoFijo = 2, hibrido = 3
    case presentacion = 4, pizarra = 5, horizontalInfinito = 6

    var nombre: String {
        switch self {
        case .libro: return "Libro (fijo)"
        case .infinitoXY: return "Infinito XY"
        case .paginadoFijo: return "Páginas (fijo)"
        case .hibrido: return "Infinito vertical"
        case .presentacion: return "Presentación"
        case .pizarra: return "Pizarra"
        case .horizontalInfinito: return "Infinito horizontal"
        }
    }

    var nombreCorto: String {
        switch self {
        case .libro, .paginadoFijo: return "Fijo"
        case .hibrido: return "∞ vertical"
        case .horizontalInfinito: return "∞ horizontal"
        case .infinitoXY, .pizarra: return "∞ XY"
        case .presentacion: return "Presentación"
        }
    }

    var simbolo: String {
        switch self {
        case .libro, .paginadoFijo: return "lock.rectangle"
        case .hibrido: return "arrow.up.and.down"
        case .horizontalInfinito: return "arrow.left.and.right"
        case .infinitoXY, .pizarra: return "arrow.up.left.and.arrow.down.right"
        case .presentacion: return "rectangle.on.rectangle"
        }
    }
}

enum UnidadLienzo: Int32, CaseIterable {
    case pixeles = 0, milimetros = 1, centimetros = 2, pulgadas = 3

    var simbolo: String {
        switch self {
        case .pixeles: return "px"
        case .milimetros: return "mm"
        case .centimetros: return "cm"
        case .pulgadas: return "in"
        }
    }
}

enum ReacomodoLienzo: Int32 { case conservar = 0, escalar = 1 }
enum EjeLienzo: Int32 { case horizontal = 0, vertical = 1 }
enum BordeLienzo: Int32 { case izquierdo = 0, derecho = 1, superior = 2, inferior = 3 }

struct ElementoTexto: Identifiable, Equatable {
    var id: String
    var texto: String
    var marco: CGRect
    var rotacion: CGFloat
    var altoAutomatico: Bool
    var desdeDictado: Bool
}

enum TipoMedio: Int32, Codable { case imagen = 0, video = 1, pdf = 2, documento = 3, audio = 4 }

struct ElementoMedio: Identifiable, Equatable {
    var id: String
    var hash: String
    var nombre: String
    var tipo: TipoMedio
    var marco: CGRect
    var rotacion: CGFloat
    var opacidad: CGFloat
    var recorte: Medios.Recorte
    var paginaFuente: Int
    var segundoInicio: Double
}

struct ReconocimientoCircuito {
    var tipo: TipoComponenteElectrico
    var confianza: Float
    var terminalA: CGPoint
    var terminalB: CGPoint
}

enum FormaReconocida: Int32 {
    case ninguna = 0, linea = 1, rectangulo = 2, elipse = 3, triangulo = 4
    case flecha = 5
}

enum TintaViewport: UInt32 {
    case principal = 0
    case secundario = 1
}