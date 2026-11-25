import Foundation

/// Solicitud de dibujo ya fusionada. El timestamp corresponde a la muestra real
/// más reciente que estará incluida en el frame; puede ser `nil` para cambios de
/// documento que no nacen de un evento del Pencil.
struct SolicitudFrameTinta: Equatable {
    let timestampEvento: TimeInterval?
}

/// Fusiona todas las muestras recibidas entre dos refrescos de pantalla.
///
/// El Pencil puede entregar varias muestras antes de que el panel pueda mostrar
/// otro frame. Encolar una presentación Metal por cada callback no crea más
/// imágenes visibles: solo llena la cola del GPU y puede agotar los drawables.
struct PlanificadorFramesTinta {
    private(set) var pendiente = false
    private(set) var timestampEvento: TimeInterval?

    mutating func solicitar(timestampEvento nuevo: TimeInterval?) {
        pendiente = true
        guard let nuevo else { return }
        if let actual = timestampEvento {
            timestampEvento = max(actual, nuevo)
        } else {
            timestampEvento = nuevo
        }
    }

    mutating func consumir() -> SolicitudFrameTinta? {
        guard pendiente else { return nil }
        let solicitud = SolicitudFrameTinta(timestampEvento: timestampEvento)
        cancelar()
        return solicitud
    }

    mutating func reponer(_ solicitud: SolicitudFrameTinta) {
        solicitar(timestampEvento: solicitud.timestampEvento)
    }

    mutating func cancelar() {
        pendiente = false
        timestampEvento = nil
    }
}

struct ResumenLatenciaTinta: Equatable {
    let cantidad: Int
    let p50: Double
    let p95: Double
    let p99: Double
    let maxima: Double

    static let vacio = ResumenLatenciaTinta(
        cantidad: 0, p50: 0, p95: 0, p99: 0, maxima: 0)
}

/// Ventana acotada de latencias evento -> fin real del trabajo en GPU.
///
/// `MTLCommandBuffer.gpuEndTime` está disponible en iOS y evita falsificar la
/// medición con el instante en que la CPU solo encoló el frame. No se etiqueta
/// como presentación física: iOS no expone aquí el callback de MTLDrawable que
/// sí aparece en otras plataformas del SDK.
struct MedidorLatenciaTinta {
    private let capacidad: Int
    private var muestrasMs: [Double] = []

    init(capacidad: Int = 240) {
        self.capacidad = max(capacidad, 1)
        muestrasMs.reserveCapacity(self.capacidad)
    }

    @discardableResult
    mutating func registrar(timestampEvento: TimeInterval,
                            tiempoCompletadoGPU: TimeInterval) -> Bool {
        let milisegundos = (tiempoCompletadoGPU - timestampEvento) * 1_000
        guard milisegundos.isFinite, milisegundos >= 0,
              milisegundos <= 10_000 else { return false }
        muestrasMs.append(milisegundos)
        if muestrasMs.count > capacidad {
            muestrasMs.removeFirst(muestrasMs.count - capacidad)
        }
        return true
    }

    var resumen: ResumenLatenciaTinta {
        guard !muestrasMs.isEmpty else { return .vacio }
        let ordenadas = muestrasMs.sorted()
        return ResumenLatenciaTinta(
            cantidad: ordenadas.count,
            p50: percentil(0.50, en: ordenadas),
            p95: percentil(0.95, en: ordenadas),
            p99: percentil(0.99, en: ordenadas),
            maxima: ordenadas.last ?? 0)
    }

    private func percentil(_ fraccion: Double, en ordenadas: [Double]) -> Double {
        guard ordenadas.count > 1 else { return ordenadas[0] }
        let posicion = min(max(fraccion, 0), 1) * Double(ordenadas.count - 1)
        let inferior = Int(floor(posicion))
        let superior = Int(ceil(posicion))
        guard inferior != superior else { return ordenadas[inferior] }
        let pesoSuperior = posicion - Double(inferior)
        return ordenadas[inferior] * (1 - pesoSuperior)
            + ordenadas[superior] * pesoSuperior
    }
}
